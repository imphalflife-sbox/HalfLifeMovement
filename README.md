> Built using Claude.

# Half-Life Movement

A Satisfactory mod that replaces base ground and air movement with a
GoldSrc (Half-Life 1) style model: ground acceleration and friction, air-strafing, and
momentum-preserving bunnyhopping.

## Setup: move the game's Jump off Space (do this first)

The mod bhops on **Space** by reading the key directly. The game's own **Jump** action is still
bound to Space by default, and that's what opens the parachute and fires the jetpack. Leave it
there and every bhop will also deploy your parachute or burn jetpack fuel.

1. In the game, open **Options → Controls** and rebind **Jump** to another key, for example
   **Left Alt**.
2. That's it: **Space** now bhops (hold it to auto-hop), and the parachute and jetpack use the
   new key.

If Jump and your bhop key are still the same key, the mod shows a warning on screen once per game
launch (turn it off with `HL.JumpKeyWarning 0` and `HL.Save`).

To bhop on a different key, set `HL.BhopKey` (e.g. `HL.BhopKey LeftAlt`) and `HL.Save`. To go back
to bhopping with the game's Jump action instead, use `HL.DedicatedBhopKey 0`. Equipment will then
fire on every hop again.

## Features

- GoldSrc ground acceleration and friction, air-strafing, classic momentum bunnyhop
- ABH (accelerated back-hopping)
- Hold-to-auto-bhop on a dedicated bhop key (Space by default), separate from the game's Jump
- Surf: ride steep, non-walkable ramps
- Conveyor momentum
- Swimming and sharking
- Momentum carry-over into the parachute (HL-style glide) and jetpack
- HL1-style footstep on every jump, so bhopping isn't silent
- Fall damage counts only vertical impact speed, and surf launches never cause fall damage
- Speedometer HUD with live horizontal speed and color-coded per-jump deltas (off by default)
- Every setting is live-tunable and can be saved

**Multiplayer:** the mod is client-side and not required on the server. It works in singleplayer
and for the host of a multiplayer game. When you join someone else's game your movement stays
vanilla, because the server is in charge of movement there. You can still join servers and
players who don't have the mod.

## Requirements

- Satisfactory
- [Satisfactory Mod Loader (SML)](https://ficsit.app/mod/SML) 3.12.0 or newer

## Installation

Install through [Satisfactory Mod Manager](https://smm.ficsit.app/) by searching for
"Half-Life Movement", or download it from [ficsit.app](https://ficsit.app/).

## Usage

Open the in-game console and use the `HL.*` commands:

| Command | What it does |
|---|---|
| `HL.Enabled 0/1` | Turn the mod off (vanilla movement) or on |
| `HL.Dump` | Print every current setting |
| `HL.Save` | Save settings so they persist across restarts |
| `HL.Debug 1` | Show the debug overlay |
| `HL.Speedometer 0/1` | Toggle the speedometer (off by default) |
| `HL.BhopKey <key>` | Key that triggers bhops (default `SpaceBar`; see Setup) |
| `HL.AutoHop`, `HL.ABH`, `HL.Surf`, `HL.Swim`, `HL.JumpSound` | Toggle individual features |
| `HL.Accelerate`, `HL.AirAccelerate`, `HL.Friction`, `HL.MaxSpeed` | Tune the movement model |
| `HL.SurfAirAccelerate` | Air acceleration while surfing (default 150, like CSS surf servers): how fast strafing into a ramp cancels a fall |

The full list of commands is in [docs/TESTING.md](docs/TESTING.md).
Settings are stored in `HalfLifeMovement.ini` in the game's Saved folder.

## Building from source

This is an [SML](https://github.com/satisfactorymodding/SatisfactoryModLoader) mod. Place the
repository in `SatisfactoryModLoader/Mods/HalfLifeMovement` of a Satisfactory modding project
and package it with Alpakit.

The mod's native hooks are only installed in packaged game builds, so it does nothing in the
editor or PIE. See [docs/TESTING.md](docs/TESTING.md) for the in-game test checklist.

## License

[MIT](LICENSE)
