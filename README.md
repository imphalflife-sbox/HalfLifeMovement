> Built using Claude.

# Half-Life Movement

A Satisfactory mod that replaces base ground and air movement with a
GoldSrc (Half-Life 1) style model: ground acceleration and friction, air-strafing, and
momentum-preserving bunnyhopping.

## Features

- GoldSrc ground acceleration and friction, air-strafing, classic momentum bunnyhop
- ABH (accelerated back-hopping)
- Hold-to-auto-bhop, with an optional dedicated bhop key
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
| `HL.AutoHop`, `HL.ABH`, `HL.Surf`, `HL.Swim`, `HL.JumpSound` | Toggle individual features |
| `HL.Accelerate`, `HL.AirAccelerate`, `HL.Friction`, `HL.MaxSpeed` | Tune the movement model |

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
