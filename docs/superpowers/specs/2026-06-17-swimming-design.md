# Half-Life Swimming + Sharking — Design

**Date:** 2026-06-17
**Status:** Approved (design); implementation plan pending

## Goal

Replicate Half-Life / GoldSrc water movement as faithfully as we can, and deliver the
**surface-swimming "sharking"** tech: at the water surface, holding the bhop key repeatedly
launches you out of the water; while out of the water you carry no water friction, so you skim
the surface fast — bunnyhopping, but on water.

The friction "reduction" is **not** a special case. It is the emergent result of physically
spending much of each cycle out of the water, exactly as in Source. Sharking and bhopping become
the same thing the moment you breach.

## Reference model

HL1 `pm_shared.c :: PM_WaterMove` (validated locally). Defining traits:

- **3D wish direction** built from view: `wishvel = forward*forwardmove + right*sidemove`, so
  looking down + forward swims downward.
- **Vertical:** `upmove` (jump) adds upward wish; **no movement input at all → drift down** (HL uses
  `wishvel.z -= 60`, "drift towards bottom").
- **Speed:** wish-speed capped at maxspeed, then `*= 0.8` (swim ≈ 80% of run).
- **Friction:** proportional, **no stop-speed floor** — `newspeed = speed − dt·speed·friction`.
  This is the floaty water deceleration (distinct from ground friction, which has a stop-speed floor).
- **Acceleration (`PM_WaterMove` form, NOT the ground/air projection form):**
  `addspeed = wishspeed − newspeed` (where `newspeed` is the post-friction **total** speed), then
  `accelspeed = accelerate·wishspeed·dt`, clamp to `addspeed`, `velocity += accelspeed · wishdir`.

## Engine / FactoryGame facts (the ground we hook)

- Satisfactory uses **stock UE `MOVE_Swimming`**, triggered by `AFGWaterVolume` (`bWaterVolume = true`,
  an `APhysicsVolume`). No drowning/oxygen system, no custom swim `CustomMovementMode`.
- `UFGCharacterMovementComponent` overrides `CalcVelocity` (which **we already hook**) and `GetMaxSpeed`;
  it does **not** override `PhysSwimming` or add water-specific logic.
- Engine `PhysSwimming` (`CharacterMovementComponent.cpp` ~4501–4618) each swim frame:
  1. computes buoyancy `NetBuoyancy = Buoyancy · ImmersionDepth()`,
  2. **clamps upward velocity** when buoyant and moving up faster than `0.33·MaxSwimSpeed`,
  3. calls `CalcVelocity(dt, Friction = 0.5·FluidFriction·Depth, isFluid=true, ...)`,
  4. adds `Velocity += GravityZ·dt·(1 − NetBuoyancy)`,
  5. moves, and exits to `MOVE_Falling` when no longer in a water volume.
- **Implication:** hooking `CalcVelocity` alone is not enough — `PhysSwimming`'s post-`CalcVelocity`
  buoyancy and **upward-velocity clamp** would fight our vertical model and the breach. We must
  neutralize that (see Risks).

## Architecture

Engine-coupled glue in `HLMovementHooks.cpp`; pure math in `HLMovementCore` (unit-tested).

- **`HLMovementCore::WaterMove(velocity, wishDir, wishSpeed, accel, friction, dt)`** — new pure
  function. Faithful `PM_WaterMove`: apply proportional 3D friction, then the `PM_WaterMove` accel
  toward `wishDir`. Full `FVector` (operates on all of X/Y/Z, unlike the X/Y-only land functions).
  No engine/world state. Unit-tested under `HalfLifeMovement.Core.*`.
- **Extend the `CalcVelocity` before/`_AFTER` hook pair** with a `MOVE_Swimming` branch (today it
  early-returns for any mode that isn't Walking/Falling). The branch:
  1. builds the 3D wish velocity from view + input (engine-coupled: needs control rotation, input),
  2. resolves vertical wish: bhop key held → swim up; **idle (no WASD and bhop key not held) → sink**
     (`bWaterSink`; matches `PM_WaterMove`, which sinks only when `!forwardmove && !sidemove && !upmove`);
  3. caps wish-speed at `MaxSpeed · WaterMaxSpeedScale`,
  4. calls `WaterMove`, stashes the result, `_AFTER` hook asserts it (same pattern as land).
- **Breach (sharking):** when `MOVE_Swimming`, **near the surface** (immersion below
  `WaterBreachImmersion`), and the **bhop key is held**, set upward velocity to `WaterBreachZ`
  (≈ a jump) so the hitbox clears the water. Clearing → engine flips to `MOVE_Falling` → existing
  air-accel + auto-bhop own it. Re-entry while the key is held re-breaches → the auto-shark chain.
- **Local-player gated** via the existing `HLIsLocalPlayerMovement`. Bhop key read via the existing
  `HLBhopHeld` (all bhop jumping — land and water — routes through the bhop key).

## Data flow (one swim frame)

```
PhysSwimming (engine)
  → [neutralize buoyancy / up-clamp]            (Risks §)
  → CalcVelocity(isFluid=true)
       before-hook: WaterMove(...) + vertical wish + (breach if surface & bhop key) → stash
       _AFTER hook: assert stashed velocity
  → move
  → leaves water → MOVE_Falling → existing air-accel + auto-bhop (sharking air phase)
```

## Settings / CVars

Per repo convention, **each** is a field in `FHLMovementSettings` + a bound `HL.*` CVar + a
Save/Load line + a `HL.Dump` entry:

| Field | Default | Meaning |
|---|---|---|
| `bSwim` | true | master toggle for HL water movement |
| `bSharking` | true | surface breach / sharking |
| `WaterMaxSpeedScale` | 0.8 | swim speed = `MaxSpeed × this` (faithful `*0.8`) |
| `WaterAccelerate` | 10.0 | water acceleration (HL `sv_accelerate` analogue) |
| `WaterFriction` | 4.0 | proportional water friction (HL `sv_friction` analogue) |
| `bWaterSink` | true | drift down when idle (escape hatch: off = neutral hover) |
| `WaterSinkSpeed` | 100.0 | idle sink wish-speed, cm/s (HL ~60 u/s ≈ 114 cm/s) |
| `WaterBreachZ` | 450.0 | upward breach impulse at the surface, cm/s (≈ a normal jump) |
| `WaterBreachImmersion` | 0.5 | breach allowed when immersion < this (capsule ≤ half submerged) |

Defaults above are the faithful/sane starting points; all are tuned in-game via `HL.*` (HL units
≈ 1.9 cm). `WaterBreachZ` may instead be sourced from the character's `JumpZVelocity` if that proves
to track jump feel better — decided during implementation.

## Risks / open implementation questions

1. **Buoyancy + upward-velocity clamp (primary risk).** `PhysSwimming` post-processes velocity after
   `CalcVelocity`. The breach impulse and idle-sink must survive it. Resolution options, to be settled
   in the plan: (a) assert swim velocity in a `TickComponent` `_AFTER` hook where it survives UE
   re-derivation — the **proven surf pattern already in this mod**; (b) tune `MaxSwimSpeed`/buoyancy so
   the clamp doesn't bite; (c) hook `PhysSwimming` directly. Prefer (a) as the known-good fallback.
2. **Water-line jitter.** Approach A may chatter at the surface as the mode flips
   Swimming↔Falling. If so, the additive fix is approach **C**: a friction taper as immersion drops
   plus a tuned breach impulse. Not built up front (YAGNI) — only if testing shows jitter.
3. **Assumption to verify FIRST in-game:** entering an `AFGWaterVolume` actually puts the local player
   in `MOVE_Swimming`. The whole approach rests on this (sourced from FG headers, not yet observed).

## Testing

- **Unit (`HalfLifeMovement.Core.*`):** `WaterMove` bleeds speed proportionally (friction, no floor);
  accel converges total speed toward wish-speed along the wish dir; Z component is carried/handled.
- **In-game (Alpakit; mod is inert in editor):**
  - Enter water → `MOVE_Swimming` (verify the core assumption).
  - Swim where you look; bhop key = ascend; no input = sink (and `bWaterSink 0` = no sink).
  - **Sharking:** at the surface, hold the bhop key → you breach repeatedly, keep/build speed, and
    skim the surface; reaching land continues seamlessly into a bhop.
  - All `HL.*` water toggles/tunables take effect live; `HL.Dump` shows them; `HL.Save` persists.

## Non-goals (YAGNI)

- No drowning / oxygen / breath (Satisfactory has none).
- No fake "skim assist" friction-cut (approach B) — sharking is emergent via real breaching.
- No surface taper (approach C) unless jitter forces it.
