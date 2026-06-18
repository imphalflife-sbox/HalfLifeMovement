# HL Parachute Glide — Design

**Date:** 2026-06-18
**Status:** Approved (design); pending implementation plan

## Problem

When the parachute is deployed, FactoryGame's `PhysParachute` decelerates the player's
horizontal velocity to a hard cap (`mStrafeMaxSpeed` ≈ 750 u/s, or `mShiftStrafeMaxSpeed`
≈ 1350 u/s while shift is held). It also converts a fraction of lateral speed *into* fall
speed (`mLateralToTerminalConversionRatio`) and applies braking deceleration
(`mBrakingDeceleration`). The result is that opening the parachute kills the momentum a
player built up via bhop / ABH.

We want the parachute to **preserve horizontal velocity and only slow the vertical
descent**, while still allowing full HL-style air control (air-strafe / steer / build
speed) during the glide.

## Background / constraints

- The parachute is a **custom movement mode**, `ECustomMovementMode::CMM_Parachute`.
  Its physics live in `UFGCharacterMovementComponent::PhysParachute`, which is **closed
  engine code** (stubbed in the SML decomp — we cannot hook it directly).
- `PhysParachute` **clamps horizontal velocity before it moves the character** each frame.
  Therefore the surf-style trick (overwrite `Self->Velocity` in the `TickComponent` _AFTER
  hook) does **not** work here: it would correct the velocity number but the displacement
  for the frame already used the clamped value, so the player still travels at 750.
- The parachute **visual / animation** is gated on the **equipment's deployed flag**
  (`AFGParachute::IsDeployed()` / the anim's `mIsParachuteDeployed`), which is replicated —
  **not** on the `CMM_Parachute` movement mode. This is what makes the chosen approach
  viable: we can take the movement mode away without (expected to) losing the chute model.
- The 750/1350 values come from the live `UFGParachuteSettings` data asset
  (header defaults are 1000/1500). Confirmed fields: `mStrafeMaxSpeed`,
  `mShiftStrafeMaxSpeed`, `mLateralToTerminalConversionRatio`, `mBrakingDeceleration`,
  `mTerminalVelocityMultiplier`.

## Approach (chosen)

**Force `MOVE_Falling` while the parachute is deployed, and separately cap downward
velocity.** In `MOVE_Falling`, the mod's existing HL pipeline (`CalcVelocity` →
air-accel / air-strafe / ABH) already owns horizontal velocity, so "full HL air control"
requires **no new horizontal math** — it is reused verbatim. The parachute is reduced to
a single job: slow the descent.

All logic lives in `HLMovementHooks.cpp` and is gated to the local player
(`HLIsLocalPlayerMovement`).

1. **Detect deploy via the equipment, not the movement mode.** Read
   `AFGParachute::IsDeployed()` off the local character's back-slot equipment. This signal
   is durable and survives us flipping the movement mode (using `IsParachuting()` would be
   circular — forcing `MOVE_Falling` makes it immediately false). Exact equipment-lookup
   API to be pinned down in the implementation plan.
2. **Force `MOVE_Falling`** each frame while deployed (override `CMM_Parachute`), in the
   `TickComponent` before-hook (or equivalent point that runs before physics). The chute
   model continues to display because its visual is driven by the equipment deployed flag.
3. **Cap descent** in the `TickComponent` _AFTER hook: if
   `Velocity.Z < -ParachuteFallSpeed`, set `Velocity.Z = -ParachuteFallSpeed`. **Downward
   only** — upward velocity (e.g. a ramp/slope-land boost landed mid-glide) is preserved.

## Settings

Following the repo convention — every feature is a field in `FHLMovementSettings` + a bound
`HL.*` CVar + a line in `SaveSettings`/`LoadSettings` + a field in `HL.Dump`:

- `bParachuteHLGlide` (bool, default `true`) → CVar `HL.ParachuteHLGlide`
  — master toggle. When off, vanilla FG parachute behavior is untouched.
- `ParachuteFallSpeed` (float, default ≈ `400.0` u/s) → CVar `HL.ParachuteFallSpeed`
  — the capped downward descent speed. Default to be matched to FG's actual slowed
  terminal velocity once measured in-game; the tunable lets us dial the glide feel.

## Testing

- **Pure math:** none required — this feature adds no new `HLMovementCore` math. The Z-cap
  is a trivial inline clamp; horizontal reuses the already-tested air model.
- **In-game (Alpakit Shipping build — the mod is inert in PIE/editor):**
  1. Build up speed via bhop/ABH (e.g. ~2000 u/s), deploy parachute → horizontal speed is
     preserved (not clamped to 750/1350); descent is slowed.
  2. While gliding, air-strafe → can steer and build speed (HL air control works).
  3. Shift held vs. not → no longer changes the horizontal cap (cap removed).
  4. **Visual check (the known risk):** confirm the chute model still renders and animates
     while in forced `MOVE_Falling`.
  5. `HL.ParachuteHLGlide 0` → reverts to vanilla parachute.
  6. `HL.ParachuteFallSpeed` adjusts descent rate live.

## Known risk & fallback

The approach assumes the chute visual is driven **purely** by the equipment deployed flag,
not secondarily by `CMM_Parachute`. **Validate in-game (test 4 above).** If forcing
`MOVE_Falling` hides the chute model or breaks its animation:

**Fallback:** leave `CMM_Parachute` active and instead neutralize FG's horizontal handling
by overriding the `UFGParachuteSettings` values at runtime — huge `mStrafeMaxSpeed` /
`mShiftStrafeMaxSpeed`, zero `mLateralToTerminalConversionRatio`, zero
`mBrakingDeceleration`. This preserves momentum + slow descent, but horizontal accel uses
FG's strafe model rather than true HL air-accel.

### UPDATE 2026-06-18 — the secondary risk materialized; pivoted to the fallback (refined)

In-game, forcing `MOVE_Falling` made the chute **instantly retract** (un-deploy animation +
normal fall) on every deploy. Root cause: FG's deploy state machine keeps `mIsDeployed`
**bidirectionally synced** to the movement mode, so the instant the mode leaves
`CMM_Parachute` the chute is retracted. The "force `MOVE_Falling`" approach is therefore
fundamentally incompatible, regardless of the visual question.

**Implemented (commit `b8505db`):** the fallback, refined to keep true HL air control.
Stay in `CMM_Parachute` (chute stays deployed) and, in the `TickComponent` _AFTER hook while
the chute is deployed:
- Neutralize FG's parachute clamp via the equipment's public `mParachuteSettings`
  (`mStrafeMaxSpeed`/`mShiftStrafeMaxSpeed` → `1e6`, `mLateralToTerminalConversionRatio` → 0,
  `mBrakingDeceleration` → 0) so `PhysParachute` moves the player at the **real** velocity
  instead of clamping the per-frame displacement. Originals are captured on entry and
  restored when the chute closes / the feature toggles off (the asset is shared).
- Own the velocity post-move (surf-style): apply our **HL air-accel** to an
  independently-tracked horizontal velocity (so FG's per-frame strafe can't run away against
  the `1e6` cap) and write it back, with a downward-only descent cap. Horizontal momentum +
  HL air-strafe are preserved; the `before`-hook is removed.

**Caveat to verify in-game:** vertical descent is governed by `PhysParachute`'s terminal
multiplier (FG's existing slow-fall), so `HL.ParachuteFallSpeed` only binds if FG's descent
is faster than it. If finer descent control is wanted, map it to `mTerminalVelocityMultiplier`
instead.
