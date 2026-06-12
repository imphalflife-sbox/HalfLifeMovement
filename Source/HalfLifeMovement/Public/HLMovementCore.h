#pragma once
#include "CoreMinimal.h"

/** Tunable physics constants (GoldSrc-style). Speeds are cm/s. */
struct FHLMoveParams
{
    float Accelerate    = 10.0f;
    float AirAccelerate = 10.0f;
    float Friction      = 4.0f;
    float StopSpeed     = 100.0f;
    float AirSpeedCap   = 30.0f;
    float MaxSpeed      = 600.0f;
    float SprintMaxSpeed= 1100.0f;
};

/**
 * Pure GoldSrc movement math. Functions take/return a full FVector velocity but only
 * modify the horizontal (X,Y) components; Z is preserved. No engine/world state.
 */
namespace HLMovementCore
{
    FVector ApplyFriction(const FVector& Velocity, float Friction, float StopSpeed, float DeltaTime);
    FVector Accelerate(const FVector& Velocity, const FVector& WishDir, float WishSpeed, float Accel, float DeltaTime);
    FVector AirAccelerate(const FVector& Velocity, const FVector& WishDir, float WishSpeed, float AirSpeedCap, float AirAccel, float DeltaTime);
    FVector ClipVelocity(const FVector& Velocity, const FVector& PlaneNormal, float Overbounce);
}
