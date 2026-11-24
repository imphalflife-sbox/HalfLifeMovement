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

## Swimming & sharking
- [ ] Entering deep water puts you in the swimming movement mode (`HL.Debug` `mode=`).
- [ ] **Swim where you look**: look down+forward dives; look up+forward climbs **when submerged** (near the surface the engine flattens upward look-swim — use the bhop key to ascend there).
- [ ] **Ascend**: hold the bhop key → you rise.
- [ ] **Idle sink**: release all input → you drift down; `HL.WaterSink 0` → neutral hover.
- [ ] Swim speed ~0.8x run; `HL.WaterMaxSpeedScale` / `HL.WaterAccelerate` / `HL.WaterFriction` tune live.
- [ ] **Sharking**: at the surface, hold the bhop key → you skip out of the water repeatedly and skim
      the surface fast, keeping/building speed; `HL.Sharking 0` disables it.
- [ ] **Shark-to-shore**: skimming into land continues seamlessly into a land bhop.
- [ ] `HL.Dump` shows the swim fields; `HL.Save` persists them across a restart.

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

## Parachute HL glide
- [ ] Build speed past 1350 (surf/bhop/ABH), deploy the **parachute** mid-air → the chute **stays
      deployed** and horizontal speed is **preserved** (not clamped to 750/1350); shift no longer
      changes a horizontal cap.
- [ ] While gliding you can **air-strafe** (HL air control) to steer/build speed; with no input the
      horizontal speed holds.
- [ ] Descent is slowed (FG's parachute slow-fall). `HL.ParachuteFallSpeed` caps downward speed
      *only if* it's lower than FG's slow-fall (otherwise FG's terminal multiplier governs descent).
- [ ] Upward velocity (e.g. landing a ramp/surf launch mid-glide) is **not** capped.
- [ ] `HL.ParachuteHLGlide 0` → vanilla parachute (re-clamps to 750/1350); `1` → glide. Closing the
      chute restores vanilla behavior (the temporary parachute-settings override is reverted).
- [ ] Build speed, use the **jetpack** → horizontal momentum is retained while thrusting.

## Persistence / config
- [ ] Change a value live (e.g. `HL.AirAccelerate 120`), `HL.Save`, fully restart the game →
      the value persists (check with `HL.Dump`).
- [ ] `HL.Dump` prints current values to the log / on-screen.

## Off really means vanilla
- [ ] Open the parachute, glide, then `HL.Enabled 0` mid-glide → the chute behaves vanilla again
      (strafe speed capped at 750/1350), not uncapped.
- [ ] `HL.Enabled 0` → steep ramps you could surf become walkable again (vanilla walkable angle);
      swim speed is vanilla. `HL.Enabled 1` → back to HL.
- [ ] `HL.Swim 0` → water movement is fully vanilla (no air-strafing in water).

## Save loads / respawn
- [ ] Play a while (bhop, ABH), exit to the main menu, load a different save → the jump footstep
      plays and ABH boosts immediately (no long dead period after loading).
- [ ] Die and respawn → bhop, surf and the parachute glide all still work.

## Multiplayer
- [ ] Host a game: your movement is HL. A friend **without** the mod can join.
- [ ] Join a friend's game (or a dedicated server) without the mod on their side: the join is
      allowed and your movement is vanilla, with no rubber-banding.

## Toggles
- [ ] `HL.Surf 0/1`, `HL.AutoHop 0/1`, `HL.MaintainHorizontalVelocity 0/1`, `HL.SlopeLandBoost 0/1`
      each change behavior as named.
- [ ] `HL.Speedometer 0/1` (default 0), `HL.SpeedometerJumps 0/1`, `HL.SpeedUnit 0/1/2` affect the HUD.
- [ ] `HL.JumpSound 1` → one surface-aware footstep per jump (alternating feet, no spam when
      auto-hopping up a ramp); `0` → silent bhops (vanilla).

---

## Full cvar reference
Each `HL.*` variable's in-game help text (type `help HL.<name>`) describes its range and default.

- **General:** `HL.Enabled HL.AutoHop HL.DedicatedBhopKey HL.BhopKey HL.JumpSound`
- **Movement model:** `HL.Accelerate HL.AirAccelerate HL.Friction HL.StopSpeed HL.AirSpeedCap
  HL.MaxSpeed HL.SprintMaxSpeed`
- **Slopes / surf:** `HL.MaintainHorizontalVelocity HL.SlopeLandBoost HL.SlopeGravity
  HL.WalkableFloorAngle HL.Surf`
- **ABH:** `HL.ABH HL.ABHBoost HL.ABHMaxGain HL.ABHDeadzone HL.ABHMinInterval`
- **Fall damage:** `HL.VerticalFallDamageOnly HL.SurfNoFallDamage`
- **Conveyors:** `HL.ConveyorMomentum`
- **Swimming:** `HL.Swim HL.Sharking HL.WaterMaxSpeedScale HL.WaterAccelerate HL.WaterFriction
  HL.WaterSink HL.WaterSinkSpeed HL.WaterBreachZ HL.WaterBreachImmersion`
- **Parachute / jetpack:** `HL.ParachuteHLGlide HL.ParachuteFallSpeed HL.ParachuteDragScale`
- **HUD / debug:** `HL.Speedometer HL.SpeedometerJumps HL.SpeedUnit HL.Debug`
- **Commands:** `HL.Dump HL.Save`

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
