#pragma once
#include "CoreMinimal.h"

/** Tunable physics constants (GoldSrc-style). Speeds are cm/s. */
struct FHLMoveParams
{
    float Accelerate    = 10.0f;
    float AirAccelerate = 100.0f;  // tuned in-game (was 10)
    float Friction      = 4.0f;
    float StopSpeed     = 100.0f;
    float AirSpeedCap   = 100.0f;  // tuned in-game (was 30)
    float MaxSpeed      = 600.0f;
    float SprintMaxSpeed= 1100.0f;
};

/**
 * Pure GoldSrc movement math. Most functions take/return a full FVector velocity but only
 * modify the horizontal (X,Y) components (Z preserved); WaterMove is the exception — it
 * operates on the full 3D velocity (swimming has vertical motion). No engine/world state.
 */
namespace HLMovementCore
{
    FVector ApplyFriction(const FVector& Velocity, float Friction, float StopSpeed, float DeltaTime);
    FVector Accelerate(const FVector& Velocity, const FVector& WishDir, float WishSpeed, float Accel, float DeltaTime);
    FVector AirAccelerate(const FVector& Velocity, const FVector& WishDir, float WishSpeed, float AirSpeedCap, float AirAccel, float DeltaTime);
    /**
     * Source/HL1 PM_AirAccelerate as written, for surfing: AirSpeedCap limits the speed reachable along
     * WishDir, but the RATE uses the uncapped WishSpeed. Steering against your own momentum is then strong,
     * which is what lets a surfer cancel a fall onto a ramp. (AirAccelerate uses the capped speed for both;
     * normal air control was tuned around that.)
     */
    FVector SurfAirAccelerate(const FVector& Velocity, const FVector& WishDir, float WishSpeed, float AirSpeedCap, float AirAccel, float DeltaTime);
    FVector WaterMove(const FVector& Velocity, const FVector& WishDir, float WishSpeed, float Accel, float Friction, float DeltaTime);
    FVector ClipVelocity(const FVector& Velocity, const FVector& PlaneNormal, float Overbounce);

    /**
     * PM_FlyMove-style clip of a full 3D velocity against the surfaces actually touched this frame
     * (unit normals). Only planes the velocity moves INTO are clipped (moving away is untouched); if
     * no single clip leaves the velocity out of every plane, it slides along the crease of two
     * planes, else stops against them. Used by surf so curved (e.g. upward-bending) ramps redirect
     * the velocity along the face really being ridden.
     */
    FVector ClipToContactPlanes(const FVector& Velocity, const TArray<FVector>& Normals);
}
