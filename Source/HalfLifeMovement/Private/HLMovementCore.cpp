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

    FVector WaterMove(const FVector& Velocity, const FVector& WishDir, float WishSpeed, float Accel, float Friction, float DeltaTime)
    {
        // Faithful PM_WaterMove. Unlike ground/air, this acts on the FULL 3D velocity and uses
        // proportional friction with NO stop-speed floor (the floaty water deceleration). Accel then
        // brings total speed toward WishSpeed along WishDir (WishDir is assumed already normalized).
        FVector V = Velocity;
        const float Speed = V.Size();
        float NewSpeed = 0.0f;
        if (Speed > 0.0f)
        {
            NewSpeed = Speed - DeltaTime * Speed * Friction;
            if (NewSpeed < 0.0f) { NewSpeed = 0.0f; }
            V *= (NewSpeed / Speed);
        }
        const float AddSpeed = WishSpeed - NewSpeed;
        if (AddSpeed <= 0.0f) { return V; }
        float AccelSpeed = Accel * WishSpeed * DeltaTime;
        if (AccelSpeed > AddSpeed) { AccelSpeed = AddSpeed; }
        V += AccelSpeed * WishDir;
        return V;
    }

    FVector ClipVelocity(const FVector& Velocity, const FVector& PlaneNormal, float Overbounce)
    {
        const float Backoff = FVector::DotProduct(Velocity, PlaneNormal) * Overbounce;
        return Velocity - PlaneNormal * Backoff;
    }

    FVector ClipToContactPlanes(const FVector& Velocity, const TArray<FVector>& Normals)
    {
        // Only the planes the velocity is moving into, de-duplicated.
        TArray<FVector, TInlineAllocator<4>> Planes;
        for (const FVector& N : Normals)
        {
            if (FVector::DotProduct(Velocity, N) >= 0.0f) { continue; }
            bool bDup = false;
            for (const FVector& P : Planes) { bDup |= FVector::DotProduct(P, N) > 0.999f; }
            if (!bDup) { Planes.Add(N); }
        }
        if (Planes.Num() == 0) { return Velocity; }

        // PM_FlyMove: find a single-plane clip that leaves the velocity out of every other plane.
        constexpr float Eps = -0.01f;
        for (int32 i = 0; i < Planes.Num(); ++i)
        {
            const FVector V = ClipVelocity(Velocity, Planes[i], 1.0f);
            bool bOk = true;
            for (int32 j = 0; j < Planes.Num() && bOk; ++j)
            {
                if (j != i && FVector::DotProduct(V, Planes[j]) < Eps) { bOk = false; }
            }
            if (bOk) { return V; }
        }
        // Two planes: slide along their crease. More: wedged, stop against them.
        if (Planes.Num() == 2)
        {
            const FVector Dir = FVector::CrossProduct(Planes[0], Planes[1]).GetSafeNormal();
            return Dir * FVector::DotProduct(Dir, Velocity);
        }
        return FVector::ZeroVector;
    }
}
