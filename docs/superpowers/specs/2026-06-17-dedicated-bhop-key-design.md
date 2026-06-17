# Dedicated bhop-key input — design

**Date:** 2026-06-17
**Status:** Approved (pending spec review)

## Problem

Bhop jumping and jump-key equipment (jetpack, parachute) share the same input. In
Satisfactory, jetpack thrust and parachute deploy are bound to the **Jump** action. The
HalfLifeMovement mod drives bhop entirely from that same action:

- the initial jump comes through vanilla Jump → `DoJump`;
- auto-bhop and the friction-skip read `Self->mIsPressingJump` (set by the Jump action).

So holding/tapping Jump to bhop also fires whatever equipment is bound to Jump. The two
behaviors cannot be used independently.

A prior attempt to fix this by **remapping the equipment off Space** (jetpack/parachute's
Enhanced Input mapping contexts rebound Space → Left Alt, commits `230d9f3`…`9e7bbb1`) was
**abandoned because it could not be made reliable** (removed in `a26378c`). We do not revisit
that route.

## Approach

**Drive bhop from a mod-polled dedicated key, independent of the game's input bindings.**

The mod polls a configurable key directly via the player controller
(`APlayerController::IsInputKeyDown`). The user rebinds the game **Jump** action off Space in
the game menu (jetpack/parachute follow it, since they are bound to that action); the mod's
bhop key (default **Space**) then drives all bhopping. Space carries nothing in the game's
input system, so it can never fire equipment.

Key insight that makes this small: the HL air-physics model in `CalcVelocity` keys off
`MovementMode`, **not** the jump key. So however the player becomes airborne — Space-bhop or a
normal game jump — the HL feel still applies. We only redirect the *bhop trigger*, not the
movement model.

### Behavior

- New setting + CVar `HL.DedicatedBhopKey` (bool, **default ON**) and `HL.BhopKey`
  (string key name, **default `"SpaceBar"`**).
- **When ON:** the bhop trigger reads **only the polled key** — never `mIsPressingJump`.
  - The game's (rebound) jump key still produces a normal vanilla single jump and fires
    equipment as usual. It does **not** auto-bhop. (Decision: leave the game jump alone; no
    interception of the jump path.)
  - Unified bhop trigger in the `TickComponent` hook:
    `if (grounded && bhopKeyHeld && (bAutoHop || bhopJustPressed)) Self->DoJump(false);`
    - `bAutoHop` true → hold-to-bhop (continuous re-jump while grounded).
    - `bAutoHop` false → tap-to-jump (one jump per fresh key press; needs a rising-edge check).
    - Same-frame safety: the first DoJump flips `MovementMode` to falling, so a second call that
      frame no-ops on the `grounded` guard.
  - The friction-skip in `CalcVelocity` switches from `Self->mIsPressingJump` to the same
    polled-key state (computed live in the hook, which has the component → owner → controller).
- **When OFF:** everything reverts to today's behavior — the game Jump action drives bhop via
  `mIsPressingJump`. Nothing changes for users who don't opt in (though default is ON).

### Out-of-the-box default-key note

Default `HL.BhopKey = "SpaceBar"` matches the user's intended setup. Until the user rebinds the
game Jump off Space, **both** the game jump and the mod poll fire on Space. This is harmless
(the first DoJump wins; our poll only adds auto-rejump, which already happens today via
`mIsPressingJump`), but the equipment conflict only actually disappears **after** the user
rebinds game Jump off Space. This is a user action in the game menu, not something the mod can
or should do.

## Components / units

All changes follow the established per-feature convention
(*a setting field + a bound `HL.*` CVar + a Save/Load line + a `HL.Dump` field*).

1. **`FHLMovementSettings`** (`Public/HLMovementSettings.h`)
   - `bool  bDedicatedBhopKey = true;`
   - `FString BhopKey = TEXT("SpaceBar");`

2. **`HLMovementConsole.cpp`**
   - `FAutoConsoleVariableRef` for `HL.DedicatedBhopKey` (bool) and `HL.BhopKey` (FString).
   - `SaveSettings` / `LoadSettings`: persist both (`SetBool`/`GetBool`, `SetString`/`GetString`).
   - `DumpSettings`: include both in the printed line.

3. **`HLMovementHooks.cpp`**
   - Helper `static bool HLBhopHeld(const UFGCharacterMovementComponent* Self, const FHLMovementSettings& S)`:
     - if `!S.bDedicatedBhopKey` → return `Self->mIsPressingJump` (current behavior).
     - else resolve `FKey(FName(*S.BhopKey))` (fallback to `EKeys::SpaceBar` if invalid), get
       `APlayerController` from `Self->GetCharacterOwner()->GetController()`, return
       `PC->IsInputKeyDown(Key)` (false if no PC).
   - `CalcVelocity` before-hook: replace `S.bAutoHop && Self->mIsPressingJump` in the
     `bSkipFriction` computation with `S.bAutoHop && HLBhopHeld(Self, S)`.
   - `TickComponent` _AFTER hook: replace the existing
     `if (S.bAutoHop && Self->mIsPressingJump && Self->IsMovingOnGround()) DoJump` block with the
     unified trigger above, using a static `bWasBhopHeld` (local player only → static is fine,
     consistent with the other statics in this hook) for the rising edge.

4. **`HalfLifeMovement.Build.cs`**
   - Verify `InputCore` is available for `FKey`/`EKeys` (likely already transitively present via
     `Engine`/`CoreUObject`). Add it only if the build needs it. We do **not** need the
     `EnhancedInput` module for this (that was the abandoned route).

## Data flow

```
Player holds Space
  → (game Jump unbound from Space → no equipment, no vanilla jump from Space)
  → TickComponent _AFTER hook: HLBhopHeld() polls PC->IsInputKeyDown(SpaceBar) == true
  → grounded && held && (autohop || justPressed) → Self->DoJump(false)
  → DoJump _AFTER hook records takeoff speed + ABH as today
  → CalcVelocity applies HL air model (keyed off MovementMode, unchanged)
```

## Testing

- **Pure-math layer (`HLMovementCore`) is unchanged** — no new unit tests needed there.
- Input polling and hook wiring are engine-coupled and only live under `!WITH_EDITOR`, so they
  are **verified in the packaged game via Alpakit**, not in PIE (per the project build/test
  workflow).
- **Primary verification (the design's one real risk):** confirm `IsInputKeyDown(SpaceBar)`
  returns true while Space is held **after Space is unbound from every game action**. If it
  does, the feature works. The `HL.Debug` overlay can surface the polled key state to confirm.
- Functional checks in-game:
  - Rebind game Jump off Space; equip nothing → Space bhops (hold = auto-bhop; with
    `HL.AutoHop 0`, tap = single jump).
  - With jetpack/parachute equipped and bound to the rebound jump key → that key fires
    equipment and does a normal jump but does **not** auto-bhop; Space still bhops without
    touching equipment.
  - `HL.DedicatedBhopKey 0` → reverts to game-jump-driven bhop.
  - `HL.Save` then restart → `HL.BhopKey` / `HL.DedicatedBhopKey` persist.

### Fallback (only if the poll proves unreliable)

If `IsInputKeyDown` does not register an unbound Space, escalate to hooking
`APlayerController::InputKey` to record raw key state, or registering a dedicated Enhanced Input
action for bhop. Not implemented unless verification fails.
