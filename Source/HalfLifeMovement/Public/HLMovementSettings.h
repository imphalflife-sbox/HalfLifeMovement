#pragma once
#include "CoreMinimal.h"
#include "HLMovementCore.h"

/** Live, in-memory mod settings. A later task wires these to the SML config menu. */
struct FHLMovementSettings
{
    bool  bEnabled = true;
    bool  bAutoHop = true;
    bool  bDedicatedBhopKey = true;            // bhop is triggered by a mod-polled key (BhopKey) instead of the game Jump action, so jump-key equipment (jetpack/parachute) is never fired by bhopping
    FString BhopKey = TEXT("SpaceBar");         // key name polled for bhop when bDedicatedBhopKey; resolved via FKey(FName), falls back to SpaceBar if invalid. Rebind the game Jump off this key in the game menu
    bool  bCarryParachute = true;
    bool  bCarryJetpack = true;
    float ParachuteDragScale = 0.25f;
    bool  bParachuteHLGlide = true;            // parachute preserves horizontal velocity + full HL air control; only the vertical descent is slowed (to ParachuteFallSpeed). Off = vanilla FG parachute (clamps horizontal to 750/1350)
    float ParachuteFallSpeed = 400.0f;         // max downward descent speed (cm/s) while the parachute glide is active; upward velocity is never capped. Tune in-game to match the desired glide feel
    // Slope/ramp handling:
    bool  bMaintainHorizontalVelocity = false; // false: velocity follows ramps (launch off crests, no free up-slope speed)
    bool  bSlopeLandBoost = true;              // landing on a slope clips/redirects fall-speed along it (downhill gain, uphill loss)
    float SlopeGravityScale = 0.0f;            // >0: gravity-along-slope while walking (downhill speed gain; modest values)
    float WalkableFloorAngle = 35.0f;          // <0: leave vanilla; >=0: max walkable slope (deg). Lower => steeper ramps become surfable (slide instead of walk)
    bool  bSurf = true;                        // while airborne, ride a non-walkable surface (own the velocity: gravity+airaccel+clip) so you can climb/launch up it
    bool  bConveyorMomentum = true;            // ride conveyor belts (additive) and keep the speed when you jump/walk off them
    bool  bABH = true;                          // accelerated back hopping (boost when pressing back over the cap)
    float ABHBoost = 0.5f;                      // sets the ABH cap: capRef = MaxSpeed*(1+this); over capRef, back-hops gain
    float ABHMaxGain = 1000.0f;                 // max horizontal speed (cm/s) a single ABH hop can add (bounds surf launches)
    float ABHDeadzone = 0.5f;                   // min facing-away strength (-dot(view,velocity), 0..1) to engage ABH; geometric like HL2, NO movement key required (so you can hold A/D to air-strafe-turn during ABH)
    float ABHMinInterval = 0.25f;               // min seconds since the previous jump required to earn an ABH boost; rapid auto-bhop re-jumps up a ramp have a tiny interval and are filtered, real spaced hops pass (measured in the DoJump hook, immune to per-frame MovementMode sampling)
    bool  bJumpSound = true;                    // play one footstep (surface-aware, FG's own footstep effect) on every jump, like HL1; otherwise bhopping is near-silent because the walk anim never reaches a foot-down notify
    bool  bVerticalFallDamageOnly = true;      // fall damage uses only vertical impact speed (zeroes horizontal), so fast bhops/ABH don't die from horizontal speed on landing
    bool  bSurfNoFallDamage = true;             // surf launches turn horizontal speed into vertical (ClipVelocity up the ramp), which VerticalFallDamageOnly can't strip; exempt the whole landing if its airborne arc touched a surf ramp
    // Swimming (HL PM_WaterMove) + sharking. Speeds in cm/s. See docs/superpowers/specs/2026-06-17-swimming-design.md.
    bool  bSwim = true;                          // HL water movement (swim where you look, jump=ascend, idle=sink)
    bool  bSharking = true;                      // surface breach: hold the bhop key at the surface to skip out of the water
    float WaterMaxSpeedScale = 0.8f;             // swim speed = MaxSpeed * this (faithful *0.8)
    float WaterAccelerate = 10.0f;               // water acceleration (sv_accelerate analogue)
    float WaterFriction = 4.0f;                  // proportional water friction, no stop-speed floor (sv_friction analogue)
    bool  bWaterSink = true;                     // drift down when idle (no WASD and bhop key not held); off = neutral hover
    float WaterSinkSpeed = 100.0f;               // idle sink wish-speed, cm/s
    float WaterBreachZ = 450.0f;                 // upward breach impulse at the surface, cm/s (~a normal jump)
    float WaterBreachImmersion = 0.5f;           // breach allowed only when ImmersionDepth() < this (capsule <= half submerged)
    FHLMoveParams Params;            // accel/airaccel/friction/cap/maxspeed/sprintmaxspeed
    bool  bSpeedometer = true;
    bool  bSpeedometerJumpSpeeds = true;
    int32 SpeedUnit = 0;             // 0=u/s(cm/s) 1=m/s 2=km/h
    bool  bDebug = false;            // on-screen debug overlay (movement mode, velocity, hook firing)

    static FHLMovementSettings& Get();
};
