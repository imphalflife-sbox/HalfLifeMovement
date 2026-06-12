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
    float SlopeGravityScale = 0.0f;            // >0: gravity-along-slope while walking (downhill speed gain; modest values)
    FHLMoveParams Params;            // accel/airaccel/friction/cap/maxspeed/sprintmaxspeed
    bool  bSpeedometer = true;
    bool  bSpeedometerJumpSpeeds = true;
    int32 SpeedUnit = 0;             // 0=u/s(cm/s) 1=m/s 2=km/h

    static FHLMovementSettings& Get();
};
