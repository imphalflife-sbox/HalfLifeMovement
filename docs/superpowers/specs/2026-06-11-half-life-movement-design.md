# Half-Life Movement — Design Spec

- **Date:** 2026-06-11
- **Mod reference:** `HalfLifeMovement` · **Display name:** "Half-Life Movement"
- **Target:** Singleplayer (use in the author's current campaign). Multiplayer is explicitly out of scope.
- **Status:** Approved design, pending spec review.

## 1. Goal

Replace Satisfactory's base air and ground movement with a faithful **GoldSrc (Half-Life 1)**
movement model — ground acceleration + friction, air-strafing, and momentum-preserving
bunnyhopping — plus an **ABH (accelerated back-hopping)** mechanic from the Source-style
air-speed cap, **hold-to-auto-bhop**, and **surf / correct sloped-collision** behavior.
All constants are tunable live via an in-game config menu. Built-up speed should **carry over**
into the parachute and jetpack so the player can build speed on the ground/ramps and spend it
traversing the world. A **speedometer** shows current speed and per-jump speed gain.

## 2. Scope

In scope:
- GoldSrc ground accel + friction, air-strafe, classic momentum bhop.
- ABH (hold S + jump gains speed) via the air-speed-cap term in air acceleration.
- Hold-to-auto-bhop (frame-perfect; pre-empts the landing friction tick).
- Surf / ramp-slide (velocity clipped to plane, no speed bleed, air control retained).
- Sprint reuses Satisfactory's existing sprint input (two-tier run/sprint ground caps).
- Momentum carry-over into parachute & jetpack (Option A: carry + light drag tuning, items
  otherwise vanilla).
- Live config menu (SML), HL-named settings.
- Speedometer HUD (current horizontal speed + optional per-jump takeoff speed & deltas,
  color-coded green/red for speed gained/lost vs the previous jump).
- Master on/off toggle (default ON) + optional live-toggle hotkey.

Out of scope (v1):
- Multiplayer / network-authoritative correctness.
- Full custom parachute/jetpack flight model (that is the "Option B" deepening, later).
- Reauthoring any other Satisfactory movement (slide, zipline, hypertube, hover, ladder,
  vault, jumping stilts) — those remain vanilla.
- Any AI-generated art. All art (mod icon, any visuals) is human-made. See
  `D:\SatisfactoryModding\AI-DISCLOSURE.md`.

## 3. Key technical constraint

`UFGCharacterMovementComponent` (extends `UCharacterMovementComponent`) is declared **`final`**,
so it cannot be subclassed and the component cannot be swapped without losing all of
Satisfactory's movement features. Therefore behavior is changed by **SML native hooks** on the
existing component's functions. Approach confirmed: **function-level override hooks** (not a
post-physics velocity rewrite, not a component replacement).

## 4. The movement model (reference math)

Quake/GoldSrc pipeline, unit-agnostic given consistent units:

```
Friction:    if speed > 0:
                 drop = max(speed, stopspeed) * friction * dt
                 vel *= max(speed - drop, 0) / speed
Accelerate:  addspeed = wishSpeed - dot(vel, wishDir)
             if addspeed <= 0: return
             accelspeed = min(accel * wishSpeed * dt, addspeed)
             vel += accelspeed * wishDir
AirAccelerate: wishSpd = min(wishSpeed, airSpeedCap)   // ~30; ABH/airstrafe engine
             addspeed = wishSpd - dot(vel, wishDir)
             if addspeed <= 0: return                   // protects forward bhop (no loss)
             accelspeed = min(airAccel * wishSpd * dt, addspeed)
             vel += accelspeed * wishDir
ClipVelocity: vel = vel - planeNormal * dot(vel, planeNormal) * overbounce   // overbounce 1.0 = slide
```

- **Air-strafe gain:** holding one strafe key (A/D, *not* W) while turning the view in the same
  direction keeps `wishDir` ~perpendicular to velocity, so `dot(vel,wishDir) ≈ 0`, the full capped
  `airSpeedCap` is added perpendicular, and `|vel|` grows. Alternate L/R to zig-zag forward and chain.
- **Forward bhop:** holding W, `wishDir` aligns with velocity, `dot` ≥ cap, `addspeed ≤ 0` →
  velocity unchanged (preserved, never reduced). The cap does NOT cap max velocity.
- **ABH:** `wishDir` backward (hold S) produces a negative-dot term that adds speed; hold sprint,
  run, jump facing backward to accelerate.
- **wishDir source:** built every air substep from **control-rotation yaw + WASD**, projected to
  the horizontal plane. This is the core of airstrafe and why W gains nothing but A/D + turn gains.

## 5. Architecture

C++ SML mod plugin. The physics math is a **pure core** with no engine state; the SML hooks are
**thin glue**. This keeps the tricky numerical code unit-testable and the hook code small.

```
Mods/HalfLifeMovement/
  HalfLifeMovement.uplugin
  Resources/Icon128.png                 (human-made placeholder)
  Source/HalfLifeMovement/
    HalfLifeMovement.Build.cs            (deps: Core, CoreUObject, Engine, FactoryGame, SML)
    Public/ Private/
      HalfLifeMovementModule.{h,cpp}     IModuleInterface; registers hooks at startup
      HLMovementCore.{h,cpp}             pure: Friction/Accelerate/AirAccelerate/ClipVelocity
      HLMovementHooks.{h,cpp}            SML native hooks -> call Core
      HLMovementConfig.{h,cpp}           tunables, surfaced in SML config menu
      HLMovementSpeedometer.{h,cpp}      HUD: current speed + per-jump speed/deltas
```

### Components

1. **`HLMovementCore`** — pure static functions (velocity/input/constants in → velocity out).
   No `this`, no component. Directly unit-testable. Owns the Section 4 math.
2. **`HLMovementHooks`** — registers SML native hooks on `UFGCharacterMovementComponent`; builds
   `wishDir` from control-rotation + input; reads config; calls Core; writes `Velocity`.
   Hook points:
   - `CalcVelocity` → override for `MOVE_Walking`/`MOVE_Falling` when enabled; else pass through.
   - `GetMaxSpeed` → effectively unbounded while airborne (defeats UE's air clamp so only
     `AirAccelerate` governs horizontal air velocity).
   - `DoJump` + landing detection → momentum jump + hold-to-auto-bhop; also captures takeoff
     speed for the speedometer.
   - `OnMovementModeChanged` → momentum carry into parachute/jetpack.
   - (Conditional) `SlideAlongSurface` → only if vanilla slide scrubs surf speed.
3. **`HLMovementConfig`** — SML config (Mods menu), HL-named labels. See Section 7.
4. **`HLMovementSpeedometer`** — draws current horizontal speed live; optional per-jump takeoff
   speed history with per-hop deltas. Each jump's readout is **color-coded by its delta vs the
   previous jump: green = speed gained, red = speed lost** (neutral/white at ~0 within a small
   epsilon). Text-based, toggleable, display-unit configurable. v1 draws on the player HUD canvas;
   may graduate to a styled UMG widget later.

## 6. Per-tick data flow

**Grounded (`MOVE_Walking`)** — `CalcVelocity` override:
1. Build `wishDir`/`wishSpeed` from WASD + yaw; `wishSpeed` capped to `sv_maxspeed`
   (or `sprint_maxspeed` if Satisfactory's sprint is held).
2. `ApplyFriction` — **unless** auto-hop pre-empts it this tick (see Section 6.1).
3. `Accelerate` toward `wishDir`. Vanilla walking accel/friction skipped.

**Airborne (`MOVE_Falling`)** — `CalcVelocity` override governs **horizontal only**:
1. Build `wishDir` from strafe + yaw.
2. `AirAccelerate`.
3. Vertical velocity & gravity stay vanilla (UE applies gravity in `PhysFalling`; Z untouched).
   `GetMaxSpeed` unbounded in air so nothing clamps built-up horizontal speed.

### 6.1 Auto-bhop & friction ordering (highest-attention item)
On the tick the player transitions Falling→Walking, if the jump input is held and `autohop` is on,
**re-jump that same tick before friction runs** — detect landing, fire `DoJump`, and skip the
friction step for that frame. Hold jump → frame-perfect chained hops that preserve momentum. This
ordering is the single trickiest requirement; it will be verified on the speedometer, not assumed.

### 6.2 Surf / sloped collisions
When airborne and contacting a surface steeper than walkable, slide along it with velocity clipped
to the plane (`ClipVelocity`, overbounce 1.0), no ground friction, `AirAccelerate` still active.
UE routes this through `SlideAlongSurface`/`ComputeSlideVector`; we ensure pure slide and only add a
`SlideAlongSurface` hook if vanilla scrubs speed.

### 6.3 Parachute / jetpack momentum carry (Option A)
- `OnMovementModeChanged`: entering `CMM_Parachute` or jetpack thrust → **preserve pre-transition
  horizontal velocity** (no reset/clamp).
- Parachute: scale horizontal drag by `parachute_drag_scale` so carried speed bleeds gradually;
  strafe input may nudge direction. Vertical (slow descent) vanilla.
- Jetpack: preserve horizontal velocity; thrust adds normally; fall-damage avoidance is vanilla.
- All other modes return early (vanilla owns them).

## 7. Config schema (SML Mods menu, HL-named)

| Setting | Default* | Purpose |
|---|---|---|
| `Enabled` | true | master on/off |
| `Hotkey` | (unbound) | live toggle vanilla↔HL |
| `sv_accelerate` | 10 | ground accel |
| `sv_airaccelerate` | 10 | air accel (airstrafe strength) |
| `sv_friction` | 4 | ground friction |
| `sv_stopspeed` | 100 | low-speed friction floor |
| `airspeedcap` | 30 | per-tick air wish clamp (ABH/strafe engine) |
| `sv_maxspeed` | tuned | ground run cap |
| `sprint_maxspeed` | tuned | sprint cap (reuses SF sprint key) |
| `autohop` | true | hold-jump auto-bhop |
| `carry_parachute` / `carry_jetpack` | true | momentum carry-over |
| `parachute_drag_scale` | <1 | how slowly carried speed bleeds while gliding |
| `speedometer_enabled` | true | draw speedometer |
| `speedometer_show_jump_speeds` | true | show per-jump takeoff speed & deltas |
| `speedometer_unit` | u/s | display unit (cm/s · m/s · u/s) |

\* GoldSrc starting points. Satisfactory uses cm/s; HL uses Quake units (~inches). Formulas are
unit-agnostic but constants (cap, maxspeed, stopspeed) need empirical tuning — the live menu is the
centerpiece for this.

## 8. Risks / unknowns

1. **Friction-ordering on landing** (§6.1) — auto-hop must pre-empt friction; correct hook
   placement is the main implementation risk.
2. **Network prediction path** — even SP runs through the saved-move/prediction system; verify no
   rubber-banding when we write velocity in the predicted path.
3. **Surf slide** — may require the optional `SlideAlongSurface` hook if vanilla scrubs speed.
4. **Unit scale** — constants need tuning (expected; handled by the config menu).

## 9. Testing plan

- **Unit tests on `HLMovementCore`** (UE automation specs, pure functions): `Accelerate` caps at
  wishspeed; `AirAccelerate` adds perpendicular speed when `wishDir⊥vel` (airstrafe gain), returns
  unchanged when `dot≥cap` (forward-bhop preserve), gains on backward `wishDir` (ABH);
  `ClipVelocity` preserves magnitude sliding a frictionless plane. Math locked via TDD before hooks.
- **In-editor manual verification (PIE):** flat ground + a test ramp; speedometer shows live speed.
  Confirm chained bhop gains, ABH gains (hold S + jump), airstrafe gains, surf holds/builds speed,
  momentum carries into parachute/jetpack.
- **In-campaign sanity** once the feel is dialed in.

## 10. Mod creation procedure

Scaffold via the **editor's SML "Create Mod" wizard** (generates a valid `.uplugin`, module, and
registration), then add the C++ classes in Section 5. The mod will live at
`D:\SatisfactoryModding\SatisfactoryModLoader\Mods\HalfLifeMovement\`. (Hand-scaffolding is possible
but the wizard avoids subtle registration mistakes.) A dedicated mod git repo can be initialized
once the plugin exists (Satisfactory docs recommend a per-mod repo).

## 11. Open follow-ups (post-v1)

- Option B: full custom parachute/jetpack aerial control (airstrafe while gliding).
- Styled UMG speedometer widget.
- Console-cvar mirror of the config (if live console tuning is wanted later).
