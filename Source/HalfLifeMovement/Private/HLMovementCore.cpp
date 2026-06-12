#include "HLMovementCore.h"

namespace HLMovementCore
{
    FVector ApplyFriction(const FVector& Velocity, float Friction, float StopSpeed, float DeltaTime)
    {
        FVector V = Velocity;
        const float Speed = FVector(V.X, V.Y, 0.0f).Size();
        if (Speed < KINDA_SMALL_NUMBER) { V.X = 0.0f; V.Y = 0.0f; return V; }
        const float Control = Speed < StopSpeed ? StopSpeed : Speed;
        const float Drop = Control * Friction * DeltaTime;
        const float NewSpeed = FMath::Max(Speed - Drop, 0.0f) / Speed;
        V.X *= NewSpeed; V.Y *= NewSpeed;
        return V;
    }

    FVector Accelerate(const FVector& Velocity, const FVector& WishDir, float WishSpeed, float Accel, float DeltaTime)
    {
        const FVector Dir(WishDir.X, WishDir.Y, 0.0f);
        const float CurrentSpeed = FVector::DotProduct(FVector(Velocity.X, Velocity.Y, 0.0f), Dir);
        const float AddSpeed = WishSpeed - CurrentSpeed;
        if (AddSpeed <= 0.0f) { return Velocity; }
        float AccelSpeed = Accel * WishSpeed * DeltaTime;
        if (AccelSpeed > AddSpeed) { AccelSpeed = AddSpeed; }
        FVector V = Velocity;
        V.X += AccelSpeed * Dir.X;
        V.Y += AccelSpeed * Dir.Y;
        return V;
    }

    FVector AirAccelerate(const FVector& Velocity, const FVector& WishDir, float WishSpeed, float AirSpeedCap, float AirAccel, float DeltaTime)
    {
        const FVector Dir(WishDir.X, WishDir.Y, 0.0f);
        const float WishSpd = FMath::Min(WishSpeed, AirSpeedCap);
        const float CurrentSpeed = FVector::DotProduct(FVector(Velocity.X, Velocity.Y, 0.0f), Dir);
        const float AddSpeed = WishSpd - CurrentSpeed;
        if (AddSpeed <= 0.0f) { return Velocity; }
        float AccelSpeed = AirAccel * WishSpd * DeltaTime;
        if (AccelSpeed > AddSpeed) { AccelSpeed = AddSpeed; }
        FVector V = Velocity;
        V.X += AccelSpeed * Dir.X;
        V.Y += AccelSpeed * Dir.Y;
        return V;
    }

    FVector ClipVelocity(const FVector& Velocity, const FVector& PlaneNormal, float Overbounce)
    {
        const float Backoff = FVector::DotProduct(Velocity, PlaneNormal) * Overbounce;
        return Velocity - PlaneNormal * Backoff;
    }
}
