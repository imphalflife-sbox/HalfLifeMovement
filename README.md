# Half-Life Movement

A singleplayer Satisfactory mod that replaces base ground and air movement with a
GoldSrc (Half-Life 1) style model — ground acceleration + friction, air-strafing, and
momentum-preserving bunnyhopping — plus ABH, hold-to-auto-bhop, surf, and a speedometer.
Built-up speed carries over into the parachute and jetpack for high-speed traversal.
Everything is live-tunable from an in-game config menu.

> Status: in development. See `docs/superpowers/specs/` for the design and
> `docs/superpowers/plans/` for the implementation plan.

## Features (target)

- GoldSrc ground accel + friction, air-strafe, classic momentum bhop
- ABH (accelerated back-hopping) via the air-speed-cap term
- Hold-to-auto-bhop (frame-perfect; pre-empts the landing friction tick)
- Surf / correct sloped-collision behavior
- Sprint reuses Satisfactory's existing sprint input
- Momentum carry-over into parachute & jetpack
- Speedometer HUD: live horizontal speed + color-coded per-jump speed deltas
- In-game config menu (HL-named settings), master on/off toggle

Singleplayer only. Multiplayer is out of scope.

## This mod uses Claude (Anthropic) as a coding assistant. All AI-suggested code is reviewed and tested by a human before it's included. No art is AI-generated — all models, textures, audio, and icons are human-made.

See `AI-DISCLOSURE.md` for details.
