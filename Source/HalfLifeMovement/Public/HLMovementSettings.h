#pragma once
#include "CoreMinimal.h"
#include "HLMovementCore.h"

/** Live, in-memory mod settings. A later task wires these to the SML config menu. */
struct FHLMovementSettings
{
    bool  bEnabled = true;
    bool  bAutoHop = true;
    bool  bCarryParachute = true;
    bool  bCarryJetpack = true;
    float ParachuteDragScale = 0.25f;
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
    float ABHDeadzone = 0.5f;                   // min back-press strength (-dot(input,velocity), 0..1) to engage ABH
    FHLMoveParams Params;            // accel/airaccel/friction/cap/maxspeed/sprintmaxspeed
    bool  bSpeedometer = true;
    bool  bSpeedometerJumpSpeeds = true;
    int32 SpeedUnit = 0;             // 0=u/s(cm/s) 1=m/s 2=km/h
    bool  bDebug = false;            // on-screen debug overlay (movement mode, velocity, hook firing)

    static FHLMovementSettings& Get();
};
