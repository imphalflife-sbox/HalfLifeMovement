# Dedicated Bhop-Key Input — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Decouple bhop from jump-key equipment by triggering bhop from a mod-polled dedicated key (`HL.BhopKey`, default Space) instead of the game Jump action.

**Architecture:** Add a settings toggle + key name (`bDedicatedBhopKey`, `BhopKey`). A new `HLBhopHeld()` helper in `HLMovementHooks.cpp` returns whether the bhop key is held — polling the key directly off the `APlayerController` (raw key state, independent of input bindings) when the toggle is on, or falling back to FG's `mIsPressingJump` when off. The two existing jump-coupled sites (the `CalcVelocity` friction-skip and the `TickComponent` auto-bhop trigger) switch to that helper, with the `TickComponent` trigger generalized to handle both hold-to-bhop and tap-to-jump.

**Tech Stack:** Unreal Engine (C++20), Satisfactory Mod Loader native hooks, `InputCore` (`FKey`/`EKeys`/`APlayerController::IsInputKeyDown`).

## Global Constraints

- **Per-feature convention (mandatory):** every new toggle/value = a field in `FHLMovementSettings` + a bound `HL.*` CVar (`FAutoConsoleVariableRef`) + a line in both `SaveSettings` and `LoadSettings` + a field in `HL.Dump`. Copy this for `bDedicatedBhopKey` and `BhopKey`.
- **Layering:** engine-coupled behavior goes in `HLMovementHooks.cpp`; the pure-math `HLMovementCore` stays untouched (no new unit tests there — it does not change).
- **Hooks are inert in the editor:** all hook code lives under `if (!WITH_EDITOR)`. The feature is **not testable in PIE/editor** — it is verified only in the packaged game via Alpakit. There is no automated test for this feature; the "test" cycle is *compile-check → in-game verify*. Do **not** invent UE automation tests for engine-coupled input polling.
- **Build/deploy (from CLAUDE.md + memory `halflife-mod-build-deploy-workflow`):** compile-check via `Ctrl+Alt+F11` Live Coding in the open editor (editor DLL only) **or** an external UBT build when the editor is closed. In-game testing requires **Alpakit Dev** on the mod, and the deploy copy **silently no-ops if the game is running** — sequence: close game → Alpakit → relaunch.
- **Git:** the mod has its **own** repo at `Mods/HalfLifeMovement/.git`. Commit there, on `main`, direct linear commits. End commit messages with the `Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>` trailer.
- **Working directory for all commands below:** `D:\SatisfactoryModding\SatisfactoryModLoader\Mods\HalfLifeMovement`.

---

### Task 1: Settings + console plumbing for the bhop key

Add the two settings fields and their full config surface (CVars, persistence, dump). After this task the CVars exist and persist, but nothing reads them yet — that's Task 2.

**Files:**
- Modify: `Source/HalfLifeMovement/Public/HLMovementSettings.h` (add two fields)
- Modify: `Source/HalfLifeMovement/Private/HLMovementConsole.cpp` (CVars + Save/Load + Dump)

**Interfaces:**
- Produces (consumed by Task 2):
  - `FHLMovementSettings::bDedicatedBhopKey` — `bool`, default `true`.
  - `FHLMovementSettings::BhopKey` — `FString`, default `TEXT("SpaceBar")`.

- [ ] **Step 1: Add the two fields to the settings struct**

In `Source/HalfLifeMovement/Public/HLMovementSettings.h`, the struct currently begins:

```cpp
struct FHLMovementSettings
{
    bool  bEnabled = true;
    bool  bAutoHop = true;
    bool  bCarryParachute = true;
```

Insert the two new fields immediately after `bAutoHop` so the input-related toggles sit together:

```cpp
struct FHLMovementSettings
{
    bool  bEnabled = true;
    bool  bAutoHop = true;
    bool  bDedicatedBhopKey = true;            // bhop is triggered by a mod-polled key (BhopKey) instead of the game Jump action, so jump-key equipment (jetpack/parachute) is never fired by bhopping
    FString BhopKey = TEXT("SpaceBar");         // key name polled for bhop when bDedicatedBhopKey; resolved via FKey(FName), falls back to SpaceBar if invalid. Rebind the game Jump off this key in the game menu
    bool  bCarryParachute = true;
```

(`FString` is available via `CoreMinimal.h`, already included at the top of the header.)

- [ ] **Step 2: Register the two CVars**

In `Source/HalfLifeMovement/Private/HLMovementConsole.cpp`, inside `RegisterCVars()`, find the auto-hop CVar:

```cpp
        static FAutoConsoleVariableRef CVarAutoHop(
            TEXT("HL.AutoHop"), S.bAutoHop, TEXT("Hold-jump auto-bhop (0/1)."));
```

Add immediately after it:

```cpp
        static FAutoConsoleVariableRef CVarDedicatedBhopKey(
            TEXT("HL.DedicatedBhopKey"), S.bDedicatedBhopKey,
            TEXT("1 = trigger bhop from a mod-polled key (HL.BhopKey) instead of the game Jump action, so jump-key equipment (jetpack/parachute) is never fired by bhopping. Rebind the game Jump off that key in the game menu. 0 = bhop uses the game Jump (original behavior)."));
        static FAutoConsoleVariableRef CVarBhopKey(
            TEXT("HL.BhopKey"), S.BhopKey,
            TEXT("Key name polled for bhop when HL.DedicatedBhopKey=1 (default SpaceBar). e.g. SpaceBar, LeftAlt, LeftControl."));
```

(`FAutoConsoleVariableRef` has an `FString&` overload, so binding `S.BhopKey` works.)

- [ ] **Step 3: Persist both in SaveSettings**

In `SaveSettings()`, find:

```cpp
        GConfig->SetBool (HLSection, TEXT("AutoHop"),                  S.bAutoHop, File);
```

Add immediately after it:

```cpp
        GConfig->SetBool  (HLSection, TEXT("DedicatedBhopKey"),        S.bDedicatedBhopKey, File);
        GConfig->SetString(HLSection, TEXT("BhopKey"),                 *S.BhopKey, File);
```

(`GConfig->SetString` takes a `const TCHAR*` value, hence `*S.BhopKey`.)

- [ ] **Step 4: Load both in LoadSettings**

In `LoadSettings()`, find:

```cpp
        GConfig->GetBool (HLSection, TEXT("AutoHop"),                  S.bAutoHop, File);
```

Add immediately after it:

```cpp
        GConfig->GetBool  (HLSection, TEXT("DedicatedBhopKey"),        S.bDedicatedBhopKey, File);
        GConfig->GetString(HLSection, TEXT("BhopKey"),                 S.BhopKey, File);
```

(`GConfig->GetString(Section, Key, FString& Out, File)` leaves `S.BhopKey` at its baked default when the key is absent — same pattern as every other Get* here.)

- [ ] **Step 5: Add both to HL.Dump**

In `DumpSettings()`, the printed line is built with `FString::Printf`. Change the format string to append `dedBhop=%d bhopKey=%s` and add the two matching args. The current call:

```cpp
        const FString Line = FString::Printf(
            TEXT("[HL] enabled=%d autohop=%d conveyor=%d abh=%d abhMinInterval=%.2f vertFallDmg=%d | accel=%.1f airaccel=%.1f friction=%.1f stopspeed=%.0f airspeedcap=%.0f maxspeed=%.0f sprint=%.0f | parachuteDrag=%.2f speedo=%d unit=%d"),
            S.bEnabled ? 1 : 0, S.bAutoHop ? 1 : 0, S.bConveyorMomentum ? 1 : 0, S.bABH ? 1 : 0,
            S.ABHMinInterval, S.bVerticalFallDamageOnly ? 1 : 0,
            S.Params.Accelerate, S.Params.AirAccelerate, S.Params.Friction, S.Params.StopSpeed,
            S.Params.AirSpeedCap, S.Params.MaxSpeed, S.Params.SprintMaxSpeed,
            S.ParachuteDragScale, S.bSpeedometer ? 1 : 0, S.SpeedUnit);
```

becomes:

```cpp
        const FString Line = FString::Printf(
            TEXT("[HL] enabled=%d autohop=%d dedBhop=%d bhopKey=%s conveyor=%d abh=%d abhMinInterval=%.2f vertFallDmg=%d | accel=%.1f airaccel=%.1f friction=%.1f stopspeed=%.0f airspeedcap=%.0f maxspeed=%.0f sprint=%.0f | parachuteDrag=%.2f speedo=%d unit=%d"),
            S.bEnabled ? 1 : 0, S.bAutoHop ? 1 : 0, S.bDedicatedBhopKey ? 1 : 0, *S.BhopKey,
            S.bConveyorMomentum ? 1 : 0, S.bABH ? 1 : 0,
            S.ABHMinInterval, S.bVerticalFallDamageOnly ? 1 : 0,
            S.Params.Accelerate, S.Params.AirAccelerate, S.Params.Friction, S.Params.StopSpeed,
            S.Params.AirSpeedCap, S.Params.MaxSpeed, S.Params.SprintMaxSpeed,
            S.ParachuteDragScale, S.bSpeedometer ? 1 : 0, S.SpeedUnit);
```

- [ ] **Step 6: Compile-check**

Editor open: press `Ctrl+Alt+F11` (Live Coding) and confirm a clean build in the Live Coding log.
Editor closed (alternative): run the external UBT build from CLAUDE.md and expect `BUILD SUCCESSFUL`:

```
"C:\Program Files\Unreal Engine - CSS\Engine\Build\BatchFiles\Build.bat" FactoryEditor Win64 Development -Project="D:\SatisfactoryModding\FactoryGame.uproject" -WaitMutex
```

Expected: compiles with no errors. (This task adds only config plumbing — no behavior yet.)

- [ ] **Step 7: Commit**

```bash
git add Source/HalfLifeMovement/Public/HLMovementSettings.h Source/HalfLifeMovement/Private/HLMovementConsole.cpp
git commit -m "feat(hlmove): add HL.DedicatedBhopKey + HL.BhopKey settings

Config plumbing only (CVars, save/load, dump); the bhop trigger reads these
in the next commit. Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 2: Drive the bhop trigger from the polled key

Add the `HLBhopHeld()` helper and route both jump-coupled sites through it, generalize the `TickComponent` trigger to hold-or-tap, and surface the polled state on the `HL.Debug` overlay for verification.

**Files:**
- Modify: `Source/HalfLifeMovement/Private/HLMovementHooks.cpp` (helper + includes + two call sites + debug write)
- Modify: `Source/HalfLifeMovement/Public/HLMovementSpeedometer.h` (one debug field)
- Modify: `Source/HalfLifeMovement/Private/HLMovementSpeedometer.cpp` (one overlay line)

**Interfaces:**
- Consumes (from Task 1): `FHLMovementSettings::bDedicatedBhopKey` (`bool`), `FHLMovementSettings::BhopKey` (`FString`).
- Produces (internal): `static bool HLBhopHeld(const UFGCharacterMovementComponent* Self, const FHLMovementSettings& S)` — true while the bhop trigger key is held; `FHLDebugState::bBhopKeyDown` (`bool`).

- [ ] **Step 1: Add the includes**

In `Source/HalfLifeMovement/Private/HLMovementHooks.cpp`, the include block ends with:

```cpp
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"
```

Add after it:

```cpp
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
```

(`GameFramework/Character.h` for `ACharacter` is already included. `InputCore` is already a Build.cs dependency, so no Build.cs change is needed.)

- [ ] **Step 2: Add the HLBhopHeld helper**

In the same file, find `HLComputeWishDir`:

```cpp
static FVector HLComputeWishDir(const UFGCharacterMovementComponent* CMC)
{
    FVector Accel = CMC->GetCurrentAcceleration();
    Accel.Z = 0.0f;
    return Accel.GetSafeNormal();
}
```

Add immediately after it:

```cpp
// True while the bhop trigger key is held. With bDedicatedBhopKey we poll a configurable key directly
// off the player controller (raw physical key state, independent of the game's input bindings), so the
// bhop trigger is decoupled from the game Jump action that jetpack/parachute are bound to. With the
// toggle off it falls back to FG's mIsPressingJump (the game Jump action) -- the original behavior.
// Local-player gating is done by the callers (HLIsLocalPlayerMovement).
static bool HLBhopHeld(const UFGCharacterMovementComponent* Self, const FHLMovementSettings& S)
{
    if (!S.bDedicatedBhopKey) { return Self->mIsPressingJump; }
    const ACharacter* Owner = Self ? Self->GetCharacterOwner() : nullptr;
    const APlayerController* PC = Owner ? Cast<APlayerController>(Owner->GetController()) : nullptr;
    if (PC == nullptr) { return false; }
    FKey Key(FName(*S.BhopKey));
    if (!Key.IsValid()) { Key = EKeys::SpaceBar; }
    return PC->IsInputKeyDown(Key);
}
```

- [ ] **Step 3: Route the CalcVelocity friction-skip through the helper**

In the `CalcVelocity` before-hook, find:

```cpp
                    const bool bSkipFriction = S.bAutoHop && Self->mIsPressingJump; // about to auto-hop; don't bleed speed
```

Replace with:

```cpp
                    const bool bSkipFriction = S.bAutoHop && HLBhopHeld(Self, S); // about to auto-hop; don't bleed speed
```

- [ ] **Step 4: Generalize the TickComponent bhop trigger**

In the `TickComponent` _AFTER hook, find the auto-bhop block:

```cpp
                // Auto-bhop: while holding jump, re-jump the moment we're grounded (launches with the
                // slope-boosted horizontal speed from above).
                if (S.bAutoHop && Self->mIsPressingJump && Self->IsMovingOnGround())
                {
                    Self->DoJump(false);
                }
```

Replace the whole block with:

```cpp
                // Bhop trigger. The held state comes from HLBhopHeld(): a mod-polled key when
                // HL.DedicatedBhopKey is on (decoupled from the game Jump action / jump-key equipment),
                // else FG's mIsPressingJump. Hold-to-bhop when HL.AutoHop is on; tap-to-jump (rising
                // edge) when it's off. The first DoJump flips MovementMode to falling, so the grounded
                // guard prevents any second trigger the same frame. Local player only -> static is fine.
                static bool bWasBhopHeld = false;
                const bool bBhopHeld = HLBhopHeld(Self, S);
                const bool bBhopJustPressed = bBhopHeld && !bWasBhopHeld;
                if (Self->IsMovingOnGround() && bBhopHeld && (S.bAutoHop || bBhopJustPressed))
                {
                    Self->DoJump(false);
                }
                bWasBhopHeld = bBhopHeld;
```

- [ ] **Step 5: Write the polled state into the debug snapshot**

Still in the `TickComponent` _AFTER hook, find the debug block near the end:

```cpp
                if (S.bDebug)
                {
                    FHLDebugState& D = FHLDebugState::Get();
                    D.MoveMode = Self->MovementMode;
                    D.CustomMode = Self->CustomMovementMode;
                    D.bOnGround = Self->IsMovingOnGround();
                    D.Vel = Self->Velocity;
                }
```

Add the `bBhopKeyDown` write (`bBhopHeld` from Step 4 is in scope here):

```cpp
                if (S.bDebug)
                {
                    FHLDebugState& D = FHLDebugState::Get();
                    D.MoveMode = Self->MovementMode;
                    D.CustomMode = Self->CustomMovementMode;
                    D.bOnGround = Self->IsMovingOnGround();
                    D.Vel = Self->Velocity;
                    D.bBhopKeyDown = bBhopHeld;
                }
```

- [ ] **Step 6: Add the debug field**

In `Source/HalfLifeMovement/Public/HLMovementSpeedometer.h`, find the `FHLDebugState` field for the jump interval:

```cpp
    float   ABHJumpInterval = 0.0f; // seconds since the previous jump (gates the boost; tiny = rapid ramp re-jump)
    static FHLDebugState& Get();
```

Insert the new field before `static FHLDebugState& Get();`:

```cpp
    float   ABHJumpInterval = 0.0f; // seconds since the previous jump (gates the boost; tiny = rapid ramp re-jump)
    bool    bBhopKeyDown = false;   // HLBhopHeld() this frame -- verify the dedicated bhop key polls correctly when the key is unbound from every game action
    static FHLDebugState& Get();
```

- [ ] **Step 7: Draw the polled state on the overlay**

In `Source/HalfLifeMovement/Private/HLMovementSpeedometer.cpp`, inside the `if (S.bDebug)` block, find the ABH line:

```cpp
            HUD->DrawText(FString::Printf(TEXT("abh=%.0f along=%.2f view=%.2f interval=%.3f"),
                D.ABHLastBoost, D.ABHAlongVel, D.ABHViewVel, D.ABHJumpInterval), C, DX, DY, Font); DY += HLLineHeight;
```

Add immediately after it:

```cpp
            HUD->DrawText(FString::Printf(TEXT("bhopKeyDown=%d dedicated=%d key=%s"),
                D.bBhopKeyDown ? 1 : 0, S.bDedicatedBhopKey ? 1 : 0, *S.BhopKey), C, DX, DY, Font); DY += HLLineHeight;
```

(`S` is the `FHLMovementSettings&` already obtained at the top of `DrawTo`.)

- [ ] **Step 8: Compile-check**

Editor open: `Ctrl+Alt+F11` (Live Coding) → clean build.
Editor closed: external UBT build (command in Global Constraints) → `BUILD SUCCESSFUL`.

- [ ] **Step 9: Package + deploy for in-game testing**

Per the build/deploy workflow: **close the game**, run **Alpakit Dev** on HalfLifeMovement (builds the Shipping target + deploys the zip), then **relaunch** the game. Verify the deployed Shipping DLL timestamp actually moved (the copy no-ops if the game was running).

- [ ] **Step 10: In-game verification (the design's one real risk first)**

Primary risk — confirm the poll sees an **unbound** key:
1. In the game's input menu, rebind **Jump** off Space to another key (e.g. Left Alt). Space is now bound to nothing.
2. In-game console: `HL.Debug 1`.
3. Hold Space and watch the overlay: `bhopKeyDown=1` while held, `0` when released, with `dedicated=1 key=SpaceBar`.

If `bhopKeyDown` stays `0` while Space is held, the poll does not register an unbound key — **stop and escalate to the fallback** in the spec (hook `APlayerController::InputKey`, or register a dedicated Enhanced Input action). Do not paper over it.

Functional checks (once the poll is confirmed):
- Equip nothing → hold Space = auto-bhop; `HL.AutoHop 0` then tap Space = single jump per press.
- Equip jetpack/parachute (bound to the rebound jump key) → that key fires equipment + does a normal jump but does **not** auto-bhop; Space still bhops without firing equipment.
- `HL.DedicatedBhopKey 0` → bhop reverts to the game Jump key (original behavior).
- `HL.BhopKey LeftAlt` → bhop now triggers on Left Alt; `HL.Dump` shows `bhopKey=LeftAlt`.
- `HL.Save`, restart the game → `HL.Dump` shows the saved `dedBhop`/`bhopKey` values.

- [ ] **Step 11: Commit**

```bash
git add Source/HalfLifeMovement/Private/HLMovementHooks.cpp Source/HalfLifeMovement/Public/HLMovementSpeedometer.h Source/HalfLifeMovement/Private/HLMovementSpeedometer.cpp
git commit -m "feat(hlmove): trigger bhop from a mod-polled key (HL.BhopKey)

Decouples bhop from the game Jump action so jump-key equipment (jetpack/
parachute) is never fired by bhopping. HLBhopHeld() polls the key off the
player controller; CalcVelocity friction-skip and the TickComponent trigger
(hold-to-bhop or tap-to-jump) both route through it. HL.Debug shows the
polled key state. Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

## Self-Review

**Spec coverage:**
- Settings `bDedicatedBhopKey` + `BhopKey` with CVars/save/load/dump → Task 1 (all steps). ✓
- Poll via `APlayerController::IsInputKeyDown`, fallback `SpaceBar` → Task 2 Step 2. ✓
- Reads only the polled key when ON, `mIsPressingJump` when OFF → Task 2 Step 2 (`HLBhopHeld`). ✓
- Game jump left alone (no interception) → confirmed by design; no task touches the jump action path. ✓
- Unified `grounded && held && (autohop || justPressed)` trigger → Task 2 Step 4. ✓
- Friction-skip switched to polled state → Task 2 Step 3. ✓
- Default ON, default key SpaceBar → Task 1 Step 1. ✓
- No EnhancedInput dependency; InputCore already present → confirmed, no Build.cs task. ✓
- Verify-poll-on-unbound-key as primary risk + fallback → Task 2 Step 10 (debug field/overlay added in Steps 5–7 to support it). ✓
- HLMovementCore unchanged / no new unit tests → stated in Global Constraints. ✓

**Placeholder scan:** No TBD/TODO/"handle edge cases"/"similar to" — every code step shows the full edit in context. ✓

**Type consistency:** `HLBhopHeld(const UFGCharacterMovementComponent*, const FHLMovementSettings&) -> bool` defined in Task 2 Step 2 and called identically in Steps 3 and 4. `bDedicatedBhopKey` (bool) and `BhopKey` (FString) named identically across Tasks 1 and 2. `FHLDebugState::bBhopKeyDown` (bool) defined in Step 6, written in Step 5, read in Step 7. ✓
