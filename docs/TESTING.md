# Half-Life Movement — Testing Checklist

Test by Alpakit-ing into the game and playing (native hooks are inert in PIE/editor).
Turn on the debug overlay with `HL.Debug 1` while testing. Settings are live via `HL.*`
console cvars; persist them with `HL.Save`.

## Load / safety
- [ ] Mod appears in the in-game mod list; game reaches the menu without crashing.
- [ ] Load a save, spawn in — no crash, movement responds.
- [ ] `HL.Enabled 0` → movement reverts to vanilla; `HL.Enabled 1` → back to HL.

## Ground movement
- [ ] Walk/run: snappy acceleration to a cap; quick stop (friction) when you release.
- [ ] Sprint (hold Satisfactory's sprint key): higher top speed than run.
- [ ] `HL.MaxSpeed` / `HL.SprintMaxSpeed` change the run/sprint caps live.

## Bunnyhopping
- [ ] **Auto-bhop**: hold the jump key → chained hops with no manual timing.
- [ ] **Forward bhop** (hold W): speed is *preserved* across straight hops (not lost, not gained).
- [ ] **Airstrafe**: in the air hold A *or* D and turn the mouse the same way → speed builds; the
      speedometer per-jump deltas go **green**.
- [ ] **ABH**: build speed past the cap, then **face ~180° away from your travel direction** and
      bhop → speed gains (geometric trigger — no movement key required; `HL.Debug` `view=` goes
      negative, `abh=` shows the per-hop boost).
- [ ] **ABH steering**: while ABH-ing, **hold A *or* D and sweep the mouse** → you turn/curve and
      keep gaining speed. (Regression guard: holding **S** during ABH must NOT lock out steering —
      the boost no longer depends on S, so strafe-turn stays available.)
- [ ] Speedometer shows live horizontal speed + recent jump speeds, color-coded green/red.

## Slopes & launches (walkable, ≤ WalkableFloorAngle)
- [ ] Walking up a slope does **not** give free speed.
- [ ] **Rampjump**: hit a ramp at high speed → you launch off the crest into the air.
- [ ] **Downhill bhop**: bhopping down a hill → green deltas (gain); uphill → red (loss).
- [ ] `HL.SlopeGravity 1` adds downhill acceleration (and a slow stand-slide on steep ground); `0` off.

## Surf (non-walkable, steeper than WalkableFloorAngle)
- [ ] Set `HL.WalkableFloorAngle` below the ramp's slope so it's surfable (overlay `walkZ` < ramp `nZ`).
- [ ] On a steep ramp the overlay shows `surfing=1`.
- [ ] **Turn into the ramp with horizontal speed** → `Vz` goes positive, you climb the face.
- [ ] Ride up and **launch off the top**, traveling along your last-contact direction.
- [ ] Speed holds on the ramp (bleeds only to gravity going up).

## Parachute / jetpack momentum carry
- [ ] Build speed (surf/bhop), deploy the **parachute** → horizontal momentum carries; you glide
      far instead of stopping. `HL.ParachuteDragScale` tunes how fast it bleeds (lower = glide further).
- [ ] Build speed, use the **jetpack** → horizontal momentum is retained while thrusting.

## Persistence / config
- [ ] Change a value live (e.g. `HL.AirAccelerate 120`), `HL.Save`, fully restart the game →
      the value persists (check with `HL.Dump`).
- [ ] `HL.Dump` prints current values to the log / on-screen.

## Toggles
- [ ] `HL.Surf 0/1`, `HL.AutoHop 0/1`, `HL.MaintainHorizontalVelocity 0/1`, `HL.SlopeLandBoost 0/1`
      each change behavior as named.
- [ ] `HL.Speedometer 0/1`, `HL.SpeedometerJumpSpeeds 0/1`, `HL.SpeedUnit 0/1/2` affect the HUD.

---

## Full cvar reference
`HL.Enabled HL.AutoHop HL.Accelerate HL.AirAccelerate HL.Friction HL.StopSpeed HL.AirSpeedCap
HL.MaxSpeed HL.SprintMaxSpeed HL.MaintainHorizontalVelocity HL.SlopeLandBoost HL.SlopeGravity
HL.WalkableFloorAngle HL.Surf HL.ParachuteDragScale HL.Speedometer HL.SpeedometerJumps
HL.SpeedUnit HL.Debug HL.Dump HL.Save`

---

## Later: full SML GUI config menu (editor-asset work)
The pause-menu Mods config GUI is **asset-based** and must be authored in the editor (I can't
generate `.uasset`s headlessly). Reference: `Mods/GameFeatures/ExampleMod/Content/ExampleModConfiguration.uasset`
and `ExampleModConfigurationStruct.uasset`.

Steps when you want the GUI (we'll pair on this):
1. Create a **Mod Configuration** asset (right-click → Modding → Mod Configuration) named e.g.
   `HalfLifeMovementConfig`. SML generates a paired config **struct** asset.
2. Add a config **property** for each setting (Bool/Float/Int) with the HL-named labels, grouped
   into sections (Air, Ground, Slope/Surf, Parachute, Speedometer).
3. In C++, on startup and on config-changed, read the generated struct via
   `UConfigManager::FillConfigurationStruct(ConfigId, ...)` and copy fields into
   `FHLMovementSettings::Get()` (same shape as `HLMovementConsole::LoadSettings`).
4. The file-based `HL.Save`/auto-load can stay as a fallback or be removed once the GUI persists.
