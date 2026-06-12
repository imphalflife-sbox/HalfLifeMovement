#include "HLMovementHooks.h"
#include "HLMovementCore.h"
#include "HLMovementSettings.h"
#include "HLMovementSpeedometer.h"
#include "FGCharacterMovementComponent.h"
#include "Patching/NativeHookManager.h"
#include "GameFramework/Character.h"
#include "GameFramework/HUD.h"
#include "FGHUD.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "Components/SceneComponent.h"

namespace HLMovementHooks
{
    // NOTE: We intentionally do NOT hook GetMaxSpeed. It is a tiny function that funchook
    // cannot patch ("Too short instructions" -> crash). It is also unnecessary: UE's lateral
    // air-speed clamp lives inside CalcVelocity, which we fully replace and Cancel() below,
    // so our AirAccelerate output is never clamped. If a residual air clamp ever appears,
    // address it via a hookable (larger) function, not GetMaxSpeed.

    // True only for the local player's own character. Prevents our movement model from
    // affecting NPCs/creatures or remote pawns that share this component class.
    static bool HLIsLocalPlayerMovement(const UFGCharacterMovementComponent* Self)
    {
        const ACharacter* Owner = Self ? Self->GetCharacterOwner() : nullptr;
        return Owner != nullptr && Owner->IsPlayerControlled() && Owner->IsLocallyControlled();
    }
}

static FVector HLComputeWishDir(const UFGCharacterMovementComponent* CMC)
{
    FVector Accel = CMC->GetCurrentAcceleration();
    Accel.Z = 0.0f;
    return Accel.GetSafeNormal();
}

namespace HLMovementHooks
{
    void RegisterHooks()
    {
        // SML native hooks are unstable at EDITOR time and crash funchook ("too short
        // instructions"), per the SML Native Hooking docs. Install them ONLY in non-editor
        // (packaged/game) builds. Consequence: this mod is tested by Alpakit-ing into the
        // game and playing there, NOT in PIE/editor (the hooks are inert in the editor).
        if (!WITH_EDITOR)
        {
        UFGCharacterMovementComponent* Sample = GetMutableDefault<UFGCharacterMovementComponent>();

        SUBSCRIBE_METHOD_VIRTUAL(UFGCharacterMovementComponent::CalcVelocity, Sample,
            [](auto& Scope, UFGCharacterMovementComponent* Self, float Dt, float Friction, bool bFluid, float BrakingDecel)
            {
                FHLMovementSettings& S = FHLMovementSettings::Get();
                if (!S.bEnabled || !HLIsLocalPlayerMovement(Self)) { return; }
                if (Self->MovementMode != MOVE_Walking && Self->MovementMode != MOVE_Falling) { return; } // defer to vanilla for slide/zipline/etc.

                // Defer to vanilla when an animation is driving movement (emotes/scripted root motion),
                // so we don't override root-motion velocity.
                if (Self->HasAnimRootMotion()) { return; }

                const FVector WishDir = HLComputeWishDir(Self);
                const float WishSpeed = Self->GetIsSprinting() ? S.Params.SprintMaxSpeed : S.Params.MaxSpeed;

                // Velocity follows the ramp surface (false) instead of being flattened to horizontal
                // (true). false => no free speed walking up slopes, and your upward velocity is kept
                // at a ramp crest so you launch into the air when you hit a ramp fast.
                Self->bMaintainHorizontalGroundVelocity = S.bMaintainHorizontalVelocity;

                // Lowering the walkable angle makes steeper ramps non-walkable, so you slide/fall on
                // them (surf) instead of walking up them. <0 leaves the vanilla angle untouched.
                if (S.WalkableFloorAngle >= 0.0f)
                {
                    Self->SetWalkableFloorAngle(S.WalkableFloorAngle);
                }

                if (Self->MovementMode == MOVE_Walking)
                {
                    FVector V = Self->Velocity;
                    const bool bSkipFriction = S.bAutoHop && Self->mIsPressingJump; // about to auto-hop; don't bleed speed
                    if (!bSkipFriction)
                    {
                        V = HLMovementCore::ApplyFriction(V, S.Params.Friction, S.Params.StopSpeed, Dt);
                    }
                    V = HLMovementCore::Accelerate(V, WishDir, WishSpeed, S.Params.Accelerate, Dt);

                    // Gravity projected onto the slope plane -> accelerate downhill / decelerate uphill.
                    // Off by default (scale 0); raising it adds Source-like downhill speed but also makes
                    // you slowly slide on steep ground while standing, so keep the scale modest.
                    if (S.SlopeGravityScale > 0.0f && Self->CurrentFloor.bWalkableFloor)
                    {
                        const FVector N = Self->CurrentFloor.HitResult.ImpactNormal;
                        if (!N.IsNearlyZero())
                        {
                            const FVector Grav(0.0f, 0.0f, Self->GetGravityZ());
                            V += FVector::VectorPlaneProject(Grav, N) * S.SlopeGravityScale * Dt;
                        }
                    }
                    Self->Velocity = V;
                }
                else // MOVE_Falling — horizontal only; gravity stays vanilla (handled in PhysFalling)
                {
                    Self->Velocity = HLMovementCore::AirAccelerate(
                        Self->Velocity, WishDir, WishSpeed, S.Params.AirSpeedCap, S.Params.AirAccelerate, Dt);

                    // Surf: if we're moving INTO a steep (non-walkable) surface, clip our velocity
                    // along it. Strafing into a ramp adds into-ramp velocity that this redirects up
                    // the ramp face (curve/launch); a direct high-speed hit launches up it too. Probes
                    // along the velocity direction, so it has no effect during open-air jumps/strafes.
                    if (S.bSurf && Self->UpdatedComponent && !Self->Velocity.IsNearlyZero())
                    {
                        if (const UWorld* World = Self->GetWorld())
                        {
                            const FVector Start = Self->UpdatedComponent->GetComponentLocation();
                            const FVector End = Start + Self->Velocity.GetSafeNormal() * S.SurfProbeDistance;
                            FCollisionQueryParams Params(FName(TEXT("HLSurfProbe")), false, Self->GetOwner());
                            FHitResult Hit;
                            if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
                            {
                                const FVector N = Hit.ImpactNormal;
                                // Steep surface (not a walkable floor) that we're moving into.
                                if (N.Z < 0.7f && FVector::DotProduct(Self->Velocity, N) < 0.0f)
                                {
                                    Self->Velocity = HLMovementCore::ClipVelocity(Self->Velocity, N, 1.0f);
                                }
                            }
                        }
                    }
                }
                Scope.Cancel(); // skip vanilla CalcVelocity for walk/air
            });

        // Record horizontal takeoff speed on each successful jump (incl. auto-hops).
        SUBSCRIBE_METHOD_VIRTUAL_AFTER(UFGCharacterMovementComponent::DoJump, Sample,
            [](bool Result, UFGCharacterMovementComponent* Self, bool bReplayingMoves)
            {
                FHLMovementSettings& S = FHLMovementSettings::Get();
                if (!S.bEnabled || !Result || !HLIsLocalPlayerMovement(Self)) { return; }
                const float HSpeed = FVector(Self->Velocity.X, Self->Velocity.Y, 0.0f).Size();
                FHLSpeedometerData::Get().RecordJump(HSpeed);
            });

        // Per-frame: slope-landing speed redirect + hold-to-auto-bhop.
        SUBSCRIBE_METHOD_VIRTUAL_AFTER(UFGCharacterMovementComponent::TickComponent, Sample,
            [](UFGCharacterMovementComponent* Self, float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
            {
                FHLMovementSettings& S = FHLMovementSettings::Get();
                if (!S.bEnabled || !HLIsLocalPlayerMovement(Self)) { return; }

                // Singleplayer: only the local player reaches here, so static last-frame state is fine.
                static uint8 LastMode = MOVE_None;
                static FVector LastVelocity = FVector::ZeroVector;

                // Landing on a slope: redirect the PRE-landing (airborne) velocity along the floor
                // plane instead of letting UE absorb the fall. ClipVelocity converts fall-speed into
                // along-slope speed -> downhill landing gains forward speed, uphill loses it (GoldSrc
                // ramp-bhop). N.Z < ~1 restricts this to actual slopes (no-op on flat ground).
                if (S.bSlopeLandBoost && LastMode == MOVE_Falling && Self->MovementMode == MOVE_Walking
                    && Self->CurrentFloor.bBlockingHit)
                {
                    const FVector N = Self->CurrentFloor.HitResult.ImpactNormal;
                    if (!N.IsNearlyZero() && N.Z < 0.999f)
                    {
                        Self->Velocity = HLMovementCore::ClipVelocity(LastVelocity, N, 1.0f);
                    }
                }

                // Auto-bhop: while holding jump, re-jump the moment we're grounded (launches with the
                // slope-boosted horizontal speed from above).
                if (S.bAutoHop && Self->mIsPressingJump && Self->IsMovingOnGround())
                {
                    Self->DoJump(false);
                }

                LastMode = Self->MovementMode;
                LastVelocity = Self->Velocity;
            });

        // Draw the speedometer each frame after the HUD draws.
        SUBSCRIBE_METHOD_VIRTUAL_AFTER(AHUD::DrawHUD, GetMutableDefault<AFGHUD>(),
            [](AHUD* HUD) { HLSpeedometer::DrawTo(HUD); });
        }
    }
}
