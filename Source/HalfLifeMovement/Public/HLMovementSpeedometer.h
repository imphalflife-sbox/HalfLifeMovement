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

/** Live debug snapshot written by the movement hooks, drawn when HL.Debug is on. Game-thread only. */
struct FHLDebugState
{
    int32   MoveMode = 0;          // EMovementMode: 1=Walking 3=Falling 6=Custom
    int32   CustomMode = 0;
    bool    bOnGround = false;
    FVector Vel = FVector::ZeroVector;
    uint32  CSVCalls = 0;          // ComputeSlideVector calls seen (local player)
    float   CSVNormalZ = 0.0f;     // last contact normal Z
    float   CSVWalkableZ = 0.0f;   // walkable floor Z threshold (surf if normalZ < this)
    bool    bCSVWouldOverride = false;
    uint32  SASCalls = 0;          // SlideAlongSurface calls seen
    float   SASNormalZ = 0.0f;
    bool    bSurfing = false;      // TickComponent surf integration active this frame
    static FHLDebugState& Get();
};

class AHUD;

namespace HLSpeedometer
{
    /** Draws the speedometer (and debug overlay) onto the HUD canvas. Call from a HUD DrawHUD hook. */
    void DrawTo(AHUD* HUD);
}
