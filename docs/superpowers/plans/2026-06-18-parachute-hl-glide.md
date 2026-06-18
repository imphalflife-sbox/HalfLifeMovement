# HL Parachute Glide Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** While the parachute is deployed, preserve the player's horizontal velocity and full HL air control, and only slow the vertical descent — instead of FG clamping horizontal speed to 750/1350 u/s.

**Architecture:** FG's `PhysParachute` (closed engine) clamps horizontal velocity *before* it moves the character, so a post-move velocity overwrite can't fix the displacement. Instead, while the parachute equipment is deployed we force the movement mode to `MOVE_Falling` in a `TickComponent` **before**-hook, so the engine runs normal falling physics and our existing `CalcVelocity` hook owns X/Y with the full HL air model. A `TickComponent` **after**-hook then caps only the downward velocity to a tunable descent speed. The chute visual stays up because it is driven by the equipment's deployed flag, not the movement mode.

**Tech Stack:** C++ / Unreal Engine (CSS fork), SML native hooks (`SUBSCRIBE_METHOD_VIRTUAL` / `_AFTER`), `FAutoConsoleVariableRef` CVars.

## Global Constraints

- **The mod is inert in PIE/editor** — SML hooks install only under `if (!WITH_EDITOR)`. Behavior is testable **only in the packaged (Shipping) game via Alpakit**. (CLAUDE.md)
- **Only `HLMovementCore` is unit-testable.** This feature is entirely engine-coupled (hooks + settings), so it has **no automation tests** — "verification" is a compile check plus an in-game procedure. Do not invent unit tests for hook code. (CLAUDE.md)
- **`Ctrl+Alt+F11` (Live Coding) compiles the editor DLL only** — use it as a *compile check*; it does NOT reach the standalone game.
- **Alpakit deploy no-ops silently if the game is running.** Sequence: close game → Alpakit → relaunch. Verify the deployed Shipping DLL timestamp actually moved. (memory `halflife-mod-build-deploy-workflow`)
- **Feature convention:** every toggle = a field in `FHLMovementSettings` + a bound `HL.*` CVar + a line in `SaveSettings`/`LoadSettings` + a field in `HL.Dump`. (CLAUDE.md)
- Commit to the mod's own git repo at `SatisfactoryModLoader/Mods/HalfLifeMovement/` on `main` (direct linear commits, no PR flow). Working directory `D:\SatisfactoryModding` is **not** a git root.

---

## File Structure

- `Source/HalfLifeMovement/Public/HLMovementSettings.h` — add `bParachuteHLGlide` (bool) and `ParachuteFallSpeed` (float) to `FHLMovementSettings`.
- `Source/HalfLifeMovement/Private/HLMovementConsole.cpp` — register `HL.ParachuteHLGlide` + `HL.ParachuteFallSpeed` CVars; add save/load lines; add to `HL.Dump`.
- `Source/HalfLifeMovement/Private/HLMovementHooks.cpp` — add `#include "Equipment/FGParachute.h"`; add `HLGetDeployedParachute()` helper; add a `TickComponent` **before**-hook that forces `MOVE_Falling`; add a descent cap at the end of the existing `TickComponent` **after**-hook.

No new files. No changes to `HLMovementSpeedometer.*` — the existing `HL.Debug` overlay already shows `MoveMode` and `CustomMode`, which is what we use to validate the mode override in-game.

**Out of scope (do not touch):** the vestigial `ParachuteDragScale` / `bCarryParachute` / `bCarryJetpack` settings (defined and persisted but never read by any hook). Leaving them avoids unrelated churn.

---

## Task 1: Settings plumbing (field + CVar + save/load + Dump)

**Files:**
- Modify: `Source/HalfLifeMovement/Public/HLMovementSettings.h`
- Modify: `Source/HalfLifeMovement/Private/HLMovementConsole.cpp`

**Interfaces:**
- Produces: `FHLMovementSettings::bParachuteHLGlide` (bool, default `true`) and `FHLMovementSettings::ParachuteFallSpeed` (float, default `400.0f`), read by the hooks in Task 2. CVars `HL.ParachuteHLGlide` and `HL.ParachuteFallSpeed`.

- [ ] **Step 1: Add the two settings fields**

In `HLMovementSettings.h`, add directly after the existing `ParachuteDragScale` line (line 14):

```cpp
    bool  bParachuteHLGlide = true;            // parachute preserves horizontal velocity + full HL air control; only the vertical descent is slowed (to ParachuteFallSpeed). Off = vanilla FG parachute (clamps horizontal to 750/1350)
    float ParachuteFallSpeed = 400.0f;         // max downward descent speed (cm/s) while the parachute glide is active; upward velocity is never capped. Tune in-game to match the desired glide feel
```

- [ ] **Step 2: Register the two CVars**

In `HLMovementConsole.cpp`, in `RegisterCVars()`, add directly after `CVarParachuteDrag` (ends at line 160):

```cpp
        static FAutoConsoleVariableRef CVarParachuteHLGlide(
            TEXT("HL.ParachuteHLGlide"), S.bParachuteHLGlide,
            TEXT("1 = deployed parachute preserves horizontal velocity + full HL air control, slowing only the vertical descent (to HL.ParachuteFallSpeed); 0 = vanilla FG parachute (clamps horizontal to 750/1350)."));
        static FAutoConsoleVariableRef CVarParachuteFallSpeed(
            TEXT("HL.ParachuteFallSpeed"), S.ParachuteFallSpeed,
            TEXT("Max downward descent speed while HL.ParachuteHLGlide is active, cm/s (default 400). Upward velocity is never capped."));
```

- [ ] **Step 3: Add save/load lines**

In `HLMovementConsole.cpp`, in `SaveSettings()`, add after the `ParachuteDragScale` SetFloat line (line 49):

```cpp
        GConfig->SetBool (HLSection, TEXT("ParachuteHLGlide"),       S.bParachuteHLGlide, File);
        GConfig->SetFloat(HLSection, TEXT("ParachuteFallSpeed"),     S.ParachuteFallSpeed, File);
```

In `LoadSettings()`, add after the `ParachuteDragScale` GetFloat line (line 98):

```cpp
        GConfig->GetBool (HLSection, TEXT("ParachuteHLGlide"),       S.bParachuteHLGlide, File);
        GConfig->GetFloat(HLSection, TEXT("ParachuteFallSpeed"),     S.ParachuteFallSpeed, File);
```

- [ ] **Step 4: Add to `HL.Dump`**

In `HLMovementConsole.cpp`, in `DumpSettings()`, extend the `parachuteDrag=%.2f` portion of the format string and its matching argument. Change the format-string fragment `parachuteDrag=%.2f speedo=%d` to:

```
parachuteDrag=%.2f paraGlide=%d paraFall=%.0f speedo=%d
```

and change the matching argument `S.ParachuteDragScale, S.bSpeedometer ? 1 : 0, S.SpeedUnit,` to:

```cpp
            S.ParachuteDragScale, S.bParachuteHLGlide ? 1 : 0, S.ParachuteFallSpeed, S.bSpeedometer ? 1 : 0, S.SpeedUnit,
```

- [ ] **Step 5: Compile check (editor DLL)**

In the open Unreal editor, press `Ctrl+Alt+F11` (Live Coding).
Expected: **Live Coding succeeds, 0 errors.** This only proves it compiles — it does not deploy to the game.

- [ ] **Step 6: Commit**

```bash
cd "D:/SatisfactoryModding/SatisfactoryModLoader/Mods/HalfLifeMovement"
git add Source/HalfLifeMovement/Public/HLMovementSettings.h Source/HalfLifeMovement/Private/HLMovementConsole.cpp
git commit -m "feat: add ParachuteHLGlide + ParachuteFallSpeed settings/CVars"
```

---

## Task 2: Parachute glide hooks (force MOVE_Falling + cap descent)

**Files:**
- Modify: `Source/HalfLifeMovement/Private/HLMovementHooks.cpp`

**Interfaces:**
- Consumes: `FHLMovementSettings::bParachuteHLGlide`, `FHLMovementSettings::ParachuteFallSpeed` (Task 1); existing `HLIsLocalPlayerMovement()`; FG `UFGCharacterMovementComponent::IsParachuting()` / `SetMovementMode()`; FG `AFGCharacterPlayer::GetEquipmentInSlot()`; FG `AFGParachute::IsDeployed()`.
- Produces: `HLGetDeployedParachute(UFGCharacterMovementComponent*) -> AFGParachute*` (returns the local player's deployed back-slot parachute, else `nullptr`).

- [ ] **Step 1: Add the parachute include**

In `HLMovementHooks.cpp`, add with the other FG includes (after line 7, `#include "FGCharacterPlayer.h"`):

```cpp
#include "Equipment/FGParachute.h"
```

(`FGParachute.h` transitively includes `Equipment/FGEquipment.h`, which declares `EEquipmentSlot`.)

- [ ] **Step 2: Add the deployed-parachute helper**

In `HLMovementHooks.cpp`, add this free function directly after `HLGetConveyorBaseVelocity` (after its closing brace at line 74), at file scope:

```cpp
// Returns the local player's parachute equipment iff it is currently DEPLOYED, else nullptr.
// Detection uses the equipment's replicated deployed flag (AFGParachute::IsDeployed) rather than the
// movement mode, because the glide forces the mode off CMM_Parachute -- so IsParachuting() would read
// false even while the chute is open. The chute is in the ES_BACK equipment slot.
static AFGParachute* HLGetDeployedParachute(UFGCharacterMovementComponent* Self)
{
    AFGCharacterPlayer* Char = Self ? Cast<AFGCharacterPlayer>(Self->GetCharacterOwner()) : nullptr;
    if (Char == nullptr) { return nullptr; }
    AFGParachute* Chute = Cast<AFGParachute>(Char->GetEquipmentInSlot(EEquipmentSlot::ES_BACK));
    return (Chute != nullptr && Chute->IsDeployed()) ? Chute : nullptr;
}
```

- [ ] **Step 3: Add the TickComponent BEFORE-hook that forces MOVE_Falling**

In `HLMovementHooks.cpp`, inside `RegisterHooks()` (the `if (!WITH_EDITOR)` block), add this new subscription immediately *before* the existing `SUBSCRIBE_METHOD_VIRTUAL_AFTER(UFGCharacterMovementComponent::TickComponent, ...)` (currently at line 349):

```cpp
        // Parachute HL glide: while the chute is DEPLOYED, take horizontal control away from FG's
        // PhysParachute (it clamps horizontal speed to 750/1350 and converts lateral speed into fall
        // speed) by forcing MOVE_Falling BEFORE the movement tick runs its physics. The engine then
        // runs normal falling physics, so our CalcVelocity hook owns X/Y with the full HL air model
        // (air-strafe/ABH), and the TickComponent _AFTER hook caps only the descent. We force it in a
        // before-hook (not after) so the mode is already Falling when physics runs this same frame --
        // robust even if FG re-asserts CMM_Parachute each frame. The chute model/anim is driven by the
        // equipment deployed flag, not the movement mode, so it stays visible. Local player only.
        SUBSCRIBE_METHOD_VIRTUAL(UFGCharacterMovementComponent::TickComponent, Sample,
            [](auto& Scope, UFGCharacterMovementComponent* Self, float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
            {
                FHLMovementSettings& S = FHLMovementSettings::Get();
                if (!S.bEnabled || !S.bParachuteHLGlide || !HLIsLocalPlayerMovement(Self)) { return; }
                if (Self->IsParachuting() && HLGetDeployedParachute(Self) != nullptr)
                {
                    Self->SetMovementMode(MOVE_Falling);
                }
            });
```

- [ ] **Step 4: Add the descent cap at the end of the existing TickComponent AFTER-hook**

In `HLMovementHooks.cpp`, inside the existing `SUBSCRIBE_METHOD_VIRTUAL_AFTER(UFGCharacterMovementComponent::TickComponent, ...)` lambda, add this block immediately before the closing `LastMode = Self->MovementMode;` line (currently line 430). It must come after the surf and slope-land blocks so it clamps the final velocity for the frame:

```cpp
                // Parachute HL glide: cap ONLY the downward descent. Horizontal velocity is owned by the
                // HL air model (CalcVelocity) because the before-hook forced MOVE_Falling, so it is left
                // untouched here. Upward velocity (e.g. a ramp/slope-land boost landed mid-glide) is
                // preserved -- we clamp only when descending faster than ParachuteFallSpeed.
                if (S.bParachuteHLGlide && HLGetDeployedParachute(Self) != nullptr)
                {
                    if (Self->Velocity.Z < -S.ParachuteFallSpeed)
                    {
                        Self->Velocity.Z = -S.ParachuteFallSpeed;
                    }
                }
```

- [ ] **Step 5: Compile check (editor DLL)**

In the open Unreal editor, press `Ctrl+Alt+F11` (Live Coding).
Expected: **Live Coding succeeds, 0 errors.** Compile-only — does not deploy.

- [ ] **Step 6: Commit**

```bash
cd "D:/SatisfactoryModding/SatisfactoryModLoader/Mods/HalfLifeMovement"
git add Source/HalfLifeMovement/Private/HLMovementHooks.cpp
git commit -m "feat: parachute HL glide -- force MOVE_Falling + cap descent while deployed"
```

---

## Task 3: Alpakit build + in-game validation + tune default

**Files:**
- (Possibly) Modify: `Source/HalfLifeMovement/Public/HLMovementSettings.h` (only if the tested-good `ParachuteFallSpeed` default differs from 400).
- (Possibly) Modify: `docs/TESTING.md` (add the parachute-glide test procedure).

**Interfaces:** none (validation task).

- [ ] **Step 1: Build + deploy via Alpakit**

Close the running game first (deploy no-ops if it's running). In the editor, run **Alpakit Dev** on the `HalfLifeMovement` mod (builds the Shipping target + zip and copies to the Steam install). Then verify the deployed Shipping DLL timestamp moved:

```bash
ls -l "/c/Program Files (x86)/Steam/steamapps/common/Satisfactory/FactoryGame/Mods/HalfLifeMovement/"
```

Expected: the Shipping `.dll` modified time is the current build time.

- [ ] **Step 2: In-game — momentum is preserved**

Launch the game. Open the console and run `HL.Debug 1` (overlay shows `MoveMode`/`CustomMode`/velocity) and `HL.Speedometer 1`.
Build up speed via bhop/ABH to well above 1350 u/s (e.g. ~2000+). Deploy the parachute mid-air.
Expected: horizontal speed is **preserved** (NOT snapped down to 750/1350); the speedometer keeps your momentum; descent visibly slows.

- [ ] **Step 3: In-game — the mode override is working**

While the chute is deployed, read the `HL.Debug` overlay.
Expected: `MoveMode` reads **Falling** and `CustomMode` reads **0/None** (not Parachute). This confirms the before-hook is winning the mode each frame.

- [ ] **Step 4: In-game — full HL air control**

While gliding, air-strafe (mouse + A/D).
Expected: you can steer and build speed exactly as in normal HL air movement. Holding shift no longer changes a horizontal cap (the 750→1350 distinction is gone).

- [ ] **Step 5: In-game — descent cap & upward preservation**

Observe the vertical speed (`Vz` on the overlay) while gliding: it should settle around `-ParachuteFallSpeed` and not exceed it downward. Run `HL.ParachuteFallSpeed 200` then `HL.ParachuteFallSpeed 600` and confirm the descent rate changes live. Glide onto a downhill ramp/launch: any upward velocity should NOT be capped.

- [ ] **Step 6: VISUAL CHECK (the known risk) — chute model still renders**

While gliding in forced `MOVE_Falling`, confirm the **parachute model is still visible and animating** (canopy deployed above the player).

- **If the chute renders correctly:** the chosen approach holds. Continue.
- **If the chute model disappears or its anim breaks** (because the visual turns out to depend on `CMM_Parachute`, not just the equipment flag): STOP and switch to the **fallback** from the spec — leave `CMM_Parachute` active (remove the before-hook's `SetMovementMode`) and instead, while deployed, override the `UFGParachuteSettings` values at runtime: set `mStrafeMaxSpeed`/`mShiftStrafeMaxSpeed` very high, `mLateralToTerminalConversionRatio = 0`, `mBrakingDeceleration = 0` (reach the settings via the deployed `AFGParachute::mParachuteSettings`). This preserves momentum + slow descent but uses FG's strafe accel instead of HL air-accel. If you take this branch, re-validate steps 2–6 and note the change in the spec's "Known risk & fallback" section.

- [ ] **Step 7: In-game — toggle reverts to vanilla**

Run `HL.ParachuteHLGlide 0`, deploy the parachute again.
Expected: vanilla FG behavior returns — horizontal speed is clamped to ~750 (or ~1350 with shift). Run `HL.ParachuteHLGlide 1` to re-enable.

- [ ] **Step 8: Tune the default & document**

If a `ParachuteFallSpeed` other than 400 felt best in steps 5, update the default in `HLMovementSettings.h` (and keep the CVar help text accurate). Add a short parachute-glide entry to `docs/TESTING.md` matching the steps above.

- [ ] **Step 9: Commit any adjustments**

```bash
cd "D:/SatisfactoryModding/SatisfactoryModLoader/Mods/HalfLifeMovement"
git add -A
git commit -m "test: parachute HL glide in-game validation + tuned default"
```

(If nothing changed in this task, skip the commit.)

---

## Notes / risks

- **Core risk (validated in Task 3 step 6):** the approach assumes the chute visual is driven purely by the equipment deployed flag, not `CMM_Parachute`. Fallback documented inline.
- **Secondary risk:** forcing `MOVE_Falling` could perturb FG's parachute deploy/un-deploy state machine. Steps 2/3/7 exercise deploy, glide, and toggle; watch for the chute auto-undeploying or mode flicker on the `HL.Debug` overlay. If the mode flickers between Falling and Parachute, the before-hook timing is being beaten by FG re-asserting the mode mid-tick — the fallback (settings override, no mode change) avoids the fight entirely.
- **No automation tests** by design — this layer is engine-coupled and only exercisable in the packaged game (CLAUDE.md). The `HLMovementCore` pure-math suite is untouched.
