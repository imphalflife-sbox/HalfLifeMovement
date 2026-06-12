#pragma once
#include "CoreMinimal.h"

/** One recorded jump's takeoff speed and its delta vs the previous jump. */
struct FHLJumpRecord
{
    float Speed = 0.0f;
    float Delta = 0.0f;
};

/** Process-wide store of recent per-jump takeoff speeds (game-thread only). */
class FHLSpeedometerData
{
public:
    static FHLSpeedometerData& Get();
    void RecordJump(float HorizontalSpeed);
    const TArray<FHLJumpRecord>& History() const { return Records; }
private:
    TArray<FHLJumpRecord> Records;       // oldest first, capped at MaxRecords
    float LastSpeed = 0.0f;
    static constexpr int32 MaxRecords = 8;
};

class AHUD;

namespace HLSpeedometer
{
    /** Draws the speedometer onto the HUD canvas. Call from a HUD DrawHUD hook. */
    void DrawTo(AHUD* HUD);
}
