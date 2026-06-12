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
    // affecting NPCs/creatures or remote pawns that share this component class. Takes the base
    // type so hooks on base-class methods (e.g. ComputeSlideVector) can reuse it.
    static bool HLIsLocalPlayerMovement(const UCharacterMovementComponent* Self)
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
                    // Air accel (airstrafe / ABH). Surf is handled by the SlideAlongSurface hook below,
                    // which constrains velocity to a ramp's plane each frame of contact: air-accel then
                    // climbs the ramp while gravity pulls down it (the surf balance).
                    Self->Velocity = HLMovementCore::AirAccelerate(
                        Self->Velocity, WishDir, WishSpeed, S.Params.AirSpeedCap, S.Params.AirAccelerate, Dt);
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

        // Surf fix (the important one): UE's ComputeSlideVector runs HandleSlopeBoosting while falling,
        // which STRIPS the upward component out of a slide into a ramp (anti-wall-climb). That stops you
        // riding up a surf ramp, and PhysFalling then re-derives velocity from the throttled move and
        // collapses your speed. During surf we override it to the plain plane-projected slide (what Quake
        // PM_FlyMove effectively does), so the character actually climbs the ramp and keeps its speed.
        // ComputeSlideVector is a base UCharacterMovementComponent method (FG does not override it).
        SUBSCRIBE_METHOD_VIRTUAL(UCharacterMovementComponent::ComputeSlideVector, Sample,
            [](auto& Scope, const UCharacterMovementComponent* Self, const FVector& Delta, float Time,
               const FVector& Normal, const FHitResult& Hit)
            {
                FHLMovementSettings& S = FHLMovementSettings::Get();
                if (S.bEnabled && S.bSurf && HLIsLocalPlayerMovement(Self)
                    && Self->MovementMode == MOVE_Falling && Normal.Z < 0.7f)
                {
                    // Plain slide, no slope-boost removal (matches UMovementComponent::ComputeSlideVector).
                    Scope.Override(FVector::VectorPlaneProject(Delta, Normal) * Time);
                }
            });

        // Surf: when the falling character slides against a surface, UE calls SlideAlongSurface with
        // the exact contact normal. Clip our velocity onto that plane every frame of contact, so the
        // ramp constrains the velocity vector. Then air-accel (above) can climb the ramp and gravity
        // (vanilla PhysFalling) pulls down it -- shallow ramps are easy to climb, steep ones you ride
        // down, and hitting one fast launches you along/up it. Only on steep (non-walkable) surfaces.
        SUBSCRIBE_METHOD_VIRTUAL(UFGCharacterMovementComponent::SlideAlongSurface, Sample,
            [](auto& Scope, UFGCharacterMovementComponent* Self, const FVector& Delta, float Time,
               const FVector& Normal, FHitResult& Hit, bool bHandleImpact)
            {
                FHLMovementSettings& S = FHLMovementSettings::Get();
                if (S.bEnabled && S.bSurf && HLIsLocalPlayerMovement(Self)
                    && Self->MovementMode == MOVE_Falling && Normal.Z < 0.7f)
                {
                    Self->Velocity = HLMovementCore::ClipVelocity(Self->Velocity, Normal, 1.0f);
                }
                // Do not cancel -- let vanilla still slide the position delta along the surface.
            });

        // Draw the speedometer each frame after the HUD draws.
        SUBSCRIBE_METHOD_VIRTUAL_AFTER(AHUD::DrawHUD, GetMutableDefault<AFGHUD>(),
            [](AHUD* HUD) { HLSpeedometer::DrawTo(HUD); });
        }
    }
}
