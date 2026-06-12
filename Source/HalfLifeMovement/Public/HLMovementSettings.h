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
    float WalkableFloorAngle = 45.0f;          // <0: leave vanilla; >=0: max walkable slope (deg). Lower => steeper ramps become surfable (slide instead of walk)
    bool  bSurf = true;                        // while airborne, slide along a non-walkable surface (bypass UE slope-boost removal) so you can ride/launch up it
    float SurfProbeDistance = 80.0f;           // (unused) legacy probe distance
    FHLMoveParams Params;            // accel/airaccel/friction/cap/maxspeed/sprintmaxspeed
    bool  bSpeedometer = true;
    bool  bSpeedometerJumpSpeeds = true;
    int32 SpeedUnit = 0;             // 0=u/s(cm/s) 1=m/s 2=km/h
    bool  bDebug = false;            // on-screen debug overlay (movement mode, velocity, hook firing)

    static FHLMovementSettings& Get();
};
