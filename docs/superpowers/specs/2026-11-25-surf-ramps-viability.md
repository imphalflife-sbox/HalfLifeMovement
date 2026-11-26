# Surf Ramps mod: viability study

Date: 2026-11-25. Status: research only. No code has been written yet.

## Verdict

**It's viable.** I'd build it as a separate content mod that depends on HalfLifeMovement. The
game already has most of the parts we need: a spline hologram base class, a pattern for saving
spline data, launch pads, and the GeometryFramework mesh tools. Two things need real work:

1. **Ramp collision has to be one smooth surface.** Vanilla belts and pipes use a chain of box
   proxies for collision (pipes: 80 cm boxes every 80 cm). Those steps would snag you on a surf
   ramp. That's the same bug that makes CS2 surf "frequently get stuck". We have to generate
   our own continuous triangle-mesh collision. **This is the riskiest part, so prototype it first.**
2. **The fall-damage rule needs a small change in HalfLifeMovement** (details below). The current
   rule protects the player too much for a mod where *landing on a ramp* is the whole point.

## What a surf ramp is (from the references)

- A sloped surface that's too steep to walk on. The player slides on it, strafes into it to
  hold on, and looks up or down to change speed. Usable slopes are between "too steep" and "too
  shallow". In HL terms: steeper than the walkable angle (our `HL.WalkableFloorAngle` = 35°),
  which means 45–60° in practice.
- The classic shape is a **floating triangular prism**: two surf faces meeting at a ridge
  (surf_2.png). Curved ramps (surf_1.png) and long winding ramps (surf_3.png) come from
  sweeping that cross-section along a path, which is exactly what a spline gives us.
- Our mod already implements the mechanic: post-move `ClipVelocity` in the `TickComponent`
  hook, plus the `ComputeSlideVector` and `SlideAlongSurface` overrides.

## Fall damage: is your understanding correct?

**Yes, for HL, and it's also true of the game's built-in model.**

- HL: `flFallVelocity = -velocity.z` is tracked while airborne. Damage is applied only when
  the player becomes grounded and that value is above the safe-fall speed.
- UE/FactoryGame: `AFGCharacterBase::Landed()` calls `CheckFallDamage(zSpeed)`
  (`FGCharacterBase.h:189-283`). UE only calls `Landed` when the character lands on a
  **walkable** floor. Hitting a surface steeper than `WalkableFloorZ` goes through
  `PhysFalling` → `SlideAlongSurface`, so the character stays in `MOVE_Falling` and never
  lands. **Hitting a surf ramp, at any speed, can't cause fall damage by itself.** The game's
  baked-in fall damage doesn't conflict with surfing.

**Where damage can still happen is when you leave the ramp.** Riding a ramp keeps your speed
pointed along the slope. On a 45° ramp, half your speed (or more) is still downward. When you
reach walkable ground (the ramp's bottom or the floor), `Landed` fires with that large Z value.

### What we have now (HalfLifeMovement)

- `HL.VerticalFallDamageOnly` sets `mFallVelocity.XY` to zero before `Landed`
  (`HLMovementHooks.cpp:544`).
- `HL.SurfNoFallDamage`: a sticky flag `GHLSurfLaunchActive` is set whenever an airborne arc
  touches surf. It makes the **whole next landing** do no damage (`HLMovementHooks.cpp:535-541`).

The second rule is too generous for this mod. Surf off a ramp kicker, fly 300 m up, fall
straight onto flat ground: no damage. That makes landing ramps pointless, because you'd never
need one.

### Recommended rule: count only the speed into the floor (normal-impact fall damage)

In the `Landed` hook we already get `Hit.ImpactNormal`. Replace the fall velocity with only the
part that goes into the floor:

```cpp
const FVector N = Hit.ImpactNormal;
const float Into = FMath::Max(0.f, -FVector::DotProduct(FallVel, N)); // speed into the floor
FallVel = FVector(0.f, 0.f, -Into); // robust whether FG reads .Z or .Size()
```

- **Flat ground:** N = (0,0,1), so this is exactly the current vertical-only rule. Nothing
  changes for bhop or ABH.
- **The bottom of a smooth landing ramp:** the surf integration clips velocity against each
  slightly-less-steep normal as you go down. By the time the surface counts as walkable,
  almost none of your speed is going into it, so you take **no damage. That's the mechanic
  you want.**
- **Launching off a ramp and falling onto flat ground:** you take full vertical damage, which
  is faithful to HL.
- **A ramp that ends in a sharp corner** (45° straight into flat floor): you take damage. Build
  a curved bottom. That's what surf mappers do, and the spline makes it easy.

With this rule `HL.SurfNoFallDamage` isn't needed any more. Keep it as an opt-in "easy mode"
that defaults to 0 once the new rule ships. This is a small maintenance change to
HalfLifeMovement, and it's worth doing even if the ramp mod never gets built. I haven't tested
it. It needs an in-game check through Alpakit.

## Technical findings

### Spline building (holograms)

| Game class | Reusable? | Notes |
|---|---|---|
| `AFGSplineHologram` | **Yes, subclass it** | Provides replicated, saved `mSplineData` (`FSplinePointData`), `mSplineComponent`, the `ESplineHologramBuildStep` steps (`FindStart → AdjustStartingPole → PlacePoleOrSnapEnding → AdjustPole`), `ResetBuildSteps`, `CanNudgeHologram`. |
| `AFGPipelineHologram` | No, use as reference only | Its `AutoRouteNoodleSpline` and other routing functions are protected and non-virtual, and they're tied to pipe connection components, pipe poles, and the 56 m `mMaxSplineLength` limit. The noodle curve is just a Hermite/Bezier between two points with their tangents, so we write our own in about 30 lines. |
| `AFGHologram` virtuals | Yes | `SetHologramLocationAndRotation`, `DoMultiStepPlacement`, `Scroll`, `ScrollRotate`, `IsValidHitResult`, `OnInvalidHitResult`, `CanTakeNextBuildStep`, `GetSupportedBuildModes`, nudge API. This is enough to drive custom air placement. |
| `UFGHologramBuildModeDescriptor` | Yes | Build modes for Noodle, Straight, and Curve, like the pipe's mode wheel. |

**Placing in mid-air** (your "draw the cursor up" idea): vanilla spline holograms put the end
point wherever the aim trace hits something. We'd instead intersect the aim ray with a
construction plane:

1. **Start:** snap to the ground or to the open end of an existing ramp.
2. **Height:** intersect the aim ray with a *vertical* plane through the start point that faces
   the camera. Looking up raises the point. Scroll fine-tunes it.
3. **End:** intersect the aim ray with a *horizontal* plane at the current height. Scroll
   changes the height difference. A real hit (terrain or another ramp) takes priority, so you
   can still snap.
4. **Chain:** each new segment starts at the previous end with the same tangent (C1
   continuity), so ramps are smooth by default. This is the "noodle" behavior you want.
5. **Roll:** `ScrollRotate` tilts the cross-section at a point. This is needed for curved,
   banked ramps like the ones in surf_2.png.

Every point can float. The game already allows floating foundations, so supports are optional
and cosmetic.

### Ramp geometry and collision (the main risk)

- **Don't use the vanilla spline collision.** `UFGSplineMeshGenerationLibrary`'s collision
  builder makes spaced boxes (`FGBuildablePipeBase.h:197`, `FGBuildableConveyorBelt.h:195`).
- **Don't build it from a chain of `USplineMeshComponent`s either.** Each segment gets its own
  convex/trimesh collision, and the seams between them are where surf snags happen.
- **Do this instead:** sweep the 2D cross-section (two faces at the ramp angle, a ridge, and
  a base) along the spline into **one welded mesh**. Use `UDynamicMeshComponent`
  (GeometryFramework is already a FactoryGame dependency) or `UProceduralMeshComponent` (in
  the engine plugins). Set complex-as-simple collision so the player capsule sweeps against
  the exact triangles. Use shared vertices and arc-length-even sampling so there are no
  T-junctions.
- Even so, Chaos can catch the capsule on internal edges of a trimesh. That's the remaining
  unknown, and it's why the first spike exists. A fallback is per-span convex hulls that
  overlap slightly.
- The ramp must block `ECC_Visibility`, because the surf floor trace in our `TickComponent`
  hook uses that channel, and `Pawn`.

### The buildable

- `AHLSurfRamp : AFGBuildable`, a normal actor rather than a lightweight instance. It holds
  `TArray<FSplinePointData>` (saved and replicated, the same pattern as
  `AFGBuildablePipeBase::mSplineData`) and the profile parameters (width, face angle,
  single- or double-sided). It regenerates its mesh on `BeginPlay` and `OnRep`.
- Variants for the MVP: separate recipes such as 45°/55°/60° × 4 m/8 m width. That covers
  "size and slope" without needing in-hologram UI. Live adjustment can come later (scroll
  modifiers or a small config widget).
- Cost should scale with length, the way pipes use `GetBaseCostMultiplier`.

### Boosters

- **The vanilla Jump Pad (`AFGBuildableJumppad`) already works as a long-range launcher.** It
  has `SetLaunchAngle`, `CalculateLaunchVelocity`, a trajectory preview (`SimulateTrajectory`),
  and power-bank charging. HL air physics doesn't clamp horizontal speed, so a jump-pad arc
  carries straight into an HL flight. **Start here, with no new code.**
- A surf-style **booster strip** (like Source's `trigger_push`) is cheap to add: a trigger
  volume on a ramp segment that adds velocity along the spline tangent. It should be a later
  phase.

### Integrating with HalfLifeMovement (small API changes to the existing mod)

- **Ramps should always be surfable.** Today any surface steeper than the global walkable
  angle is surfable. Add a component tag (`HLSurf`) check. A tagged surface returns
  non-walkable through a hook on `UCharacterMovementComponent::IsWalkable` (a virtual), and
  it passes the surf checks no matter its angle. Players could then keep a vanilla-feeling
  walkable angle and still surf ramps.
- **Normal-impact fall damage** (see above).
- Declare the dependency in the ramp mod's `.uplugin`: `HalfLifeMovement` at a minimum version.

## Constraints and risks

| Risk | Severity | Mitigation |
|---|---|---|
| Collision seams or snags on generated ramps | **High** | Spike first (Phase 1). Use one welded mesh and complex-as-simple collision. Fallback: overlapping convex spans. |
| **Multiplayer:** HL hooks don't run on clients (`HLIsLocalPlayerMovement` excludes `NM_Client`; see the go-live hardening commit). Ramps replicate, but only the host can surf. Content mods also need `RequiredOnRemote=true`. | **High (design)** | Ship as single-player / host-only and say so. Server-side HL simulation for remote players is a separate, large project. |
| Hologram UX with no hit surface | Medium | Plane-intersection placement (above), plus clear on-screen height and length readouts. |
| Testing only works in a packaged Alpakit build (`!WITH_EDITOR` hooks) | Medium | Put geometry generation in a pure helper with automation tests, the same approach as `HLMovementCore`. That's the part that can be unit-tested. |
| Game updates changing `AFGSplineHologram` internals | Medium | Keep our subclass thin and own the routing and mesh code. |
| Editor/content work: BP subclasses, descriptors, recipes, a schematic, icons, materials | Medium | This is new territory compared with the current C++-only mod. Budget time for it. |
| Performance of long procedural ramps | Low | One actor per ramp, regenerated only on build, load, or edit. |

## Recommended phases

0. **HalfLifeMovement maintenance release:** normal-impact fall damage plus the `HLSurf` tag and
   `IsWalkable` hook. Small, can be shipped and tested on its own, and useful even without the
   new mod.
1. **Collision spike** (proves the riskiest part): a console command `Surf.SpawnTestRamp`
   that spawns a hard-coded curved prism ramp at the aim point. Check surf feel, snags, and a
   landing on the curved bottom with no damage. **Stop here if it can't be made smooth.**
2. **Buildable:** `AHLSurfRamp` with saved and replicated spline data, mesh sweep, recipes and
   descriptors, and a fixed-profile variant set.
3. **Hologram:** start, height, end, and chain on construction planes, Noodle/Straight build
   modes, roll.
4. **Transport polish:** booster strips, trajectory-guided placement of landing ramps (reuse
   `AFGBuildableJumppad::SimulateTrajectory` so a landing ramp can be placed where a jump pad
   arc comes down), and live size/slope adjustment.
