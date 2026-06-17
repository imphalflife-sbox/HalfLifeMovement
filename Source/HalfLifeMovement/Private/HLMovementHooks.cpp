#include "HLMovementHooks.h"
#include "HLMovementCore.h"
#include "HLMovementSettings.h"
#include "HLMovementSpeedometer.h"
#include "FGCharacterMovementComponent.h"
#include "Buildables/FGBuildableConveyorBelt.h"
#include "Patching/NativeHookManager.h"
#include "GameFramework/Character.h"
#include "GameFramework/HUD.h"
#include "FGHUD.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"

namespace HLMovementHooks
{
    // NOTE: We intentionally do NOT hook GetMaxSpeed. It is a tiny function that funchook
    // cannot patch ("Too short instructions" -> crash). It is also unnecessary: UE's lateral
    // air-speed clamp lives inside CalcVelocity, whose output we overwrite (we set Velocity in a
    // CalcVelocity _AFTER hook below), so our AirAccelerate output is never clamped. If a residual
    // air clamp ever appears, address it via a hookable (larger) function, not GetMaxSpeed.

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

// Velocity imparted by the conveyor belt the player is standing on, in world space (zero when off
// a belt, airborne, or the feature is disabled). We keep this OUT of the HL friction/accel calc and
// re-add it after, so riding a belt is clean and leaving it folds the belt speed into the player's
// own velocity (emergent momentum preservation). Belts only (v1); callers are already gated to the
// local player. CurrentFloor is the surface the character is standing on -> naturally empty in air.
static FVector HLGetConveyorBaseVelocity(UFGCharacterMovementComponent* Self)
{
    const FHLMovementSettings& S = FHLMovementSettings::Get();
    if (!S.bConveyorMomentum || !Self->CurrentFloor.bBlockingHit) { return FVector::ZeroVector; }
    AFGBuildableConveyorBelt* Belt = Cast<AFGBuildableConveyorBelt>(Self->CurrentFloor.HitResult.GetActor());
    if (Belt == nullptr) { return FVector::ZeroVector; }
    return Belt->GetVelocityForBase(Self->GetOwner(), Self->CurrentFloor.HitResult.GetComponent());
}

// Surf: the TickComponent (post-move) hook maintains the player's velocity while in contact with a
// non-walkable ramp -- gravity + air-accel + clip-to-plane, Quake PM_FlyMove style -- and asserts it
// where it survives UE's velocity re-derivation (verified by HL.SurfTestVz). GHLWasSurfing is shared
// so CalcVelocity can skip its own air-accel during surf (the integration owns velocity then).
static bool GHLWasSurfing = false;

// CalcVelocity computes the HL velocity from the PRE-call velocity and stashes it here; the
// CalcVelocity _AFTER hook applies it once vanilla FG CalcVelocity has run. We let FG's override run
// (instead of Cancel()-ing it) so its land/anim bookkeeping -- the pre-land prediction that drives the
// landing camera dip + audio -- keeps being serviced; skipping it leaves that effect re-triggering
// while grounded. Singleplayer local player only, not re-entrant -> a static is fine.
static FVector GHLDesiredVelocity = FVector::ZeroVector;
static bool    GHLApplyDesiredVelocity = false;

// Conveyor belt velocity for the current frame, captured in the CalcVelocity before-hook and
// re-added in the _AFTER hook (mirrors FG's mAddedVelocity layering). Local player only -> static.
static FVector GHLBeltVelocity = FVector::ZeroVector;

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
                GHLApplyDesiredVelocity = false; // armed only on the compute paths below
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

                // Conveyor momentum: lift the belt's contribution out so HL friction/accel act only
                // on the player's own velocity; the _AFTER hook re-adds it. Leaving the belt (B->0)
                // folds the belt speed into the own velocity -> momentum carries into the air.
                const FVector BeltVel = HLGetConveyorBaseVelocity(Self);
                GHLBeltVelocity = BeltVel;
                const FVector OwnVel = Self->Velocity - BeltVel;
                if (S.bDebug)
                {
                    FHLDebugState& D = FHLDebugState::Get();
                    D.BeltSpeed = BeltVel.Size();
                    D.bOnBelt = !BeltVel.IsNearlyZero();
                }

                FVector NewVel;
                if (Self->MovementMode == MOVE_Walking)
                {
                    FVector V = OwnVel;
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
                    NewVel = V;
                }
                else // MOVE_Falling — horizontal only; gravity stays vanilla (handled in PhysFalling)
                {
                    // Air accel (airstrafe / ABH). During surf, the TickComponent integration owns the
                    // velocity (gravity+airaccel+clip), so skip air-accel here and keep the pre-call
                    // velocity (TickComponent overwrites it post-move) to avoid double-applying it.
                    NewVel = GHLWasSurfing
                        ? OwnVel
                        : HLMovementCore::AirAccelerate(
                            OwnVel, WishDir, WishSpeed, S.Params.AirSpeedCap, S.Params.AirAccelerate, Dt);
                }

                // Stash the HL velocity and let vanilla CalcVelocity proceed (do NOT Cancel): FG's
                // override runs its land/anim bookkeeping, then the _AFTER hook overwrites the velocity
                // it produced with NewVel so the HL model still wins (FG's air clamp is discarded).
                GHLDesiredVelocity = NewVel;
                GHLApplyDesiredVelocity = true;
            });

        // Second half of the velocity replacement: apply the HL velocity AFTER vanilla FG CalcVelocity
        // has run (and done its land/anim bookkeeping). This pair (compute-before, apply-after) replaces
        // the old Scope.Cancel(), which skipped FG's bookkeeping and left the landing camera/audio
        // re-triggering while grounded.
        SUBSCRIBE_METHOD_VIRTUAL_AFTER(UFGCharacterMovementComponent::CalcVelocity, Sample,
            [](UFGCharacterMovementComponent* Self, float Dt, float Friction, bool bFluid, float BrakingDecel)
            {
                if (GHLApplyDesiredVelocity && HLIsLocalPlayerMovement(Self))
                {
                    Self->Velocity = GHLDesiredVelocity + GHLBeltVelocity;
                }
                GHLApplyDesiredVelocity = false;
            });

        // Record horizontal takeoff speed on each successful jump (incl. auto-hops).
        SUBSCRIBE_METHOD_VIRTUAL_AFTER(UFGCharacterMovementComponent::DoJump, Sample,
            [](bool Result, UFGCharacterMovementComponent* Self, bool bReplayingMoves)
            {
                FHLMovementSettings& S = FHLMovementSettings::Get();
                if (!S.bEnabled || !Result || !HLIsLocalPlayerMovement(Self)) { return; }

                // Accelerated Back Hopping. The technique: face AWAY from your travel direction
                // (turn 180) and hold S, which drives your input TOWARD your travel direction. So ABH =
                // input pointing ALONG your velocity while your view points AWAY from it. That facing
                // test is what separates ABH from a normal forward (W) bhop -- both drive input along
                // velocity, but forward bhop faces along it. When over the cap we add a boost ALONG the
                // velocity, scaled by how directly the input drives along it and clamped to a max, so
                // strafing/no-keys/slope-noise can't fire it and surf speeds can't explode. Uses the raw
                // input vector (zero when no key held). Before RecordJump so the speedometer reflects it.
                if (S.bABH)
                {
                    FVector InputV = Self->GetLastInputVector();
                    InputV.Z = 0.0f;
                    const float InputMag = InputV.Size();
                    const FVector VelH(Self->Velocity.X, Self->Velocity.Y, 0.0f);
                    const float Speed = VelH.Size();
                    const float CapRef = S.Params.MaxSpeed * (1.0f + S.ABHBoost);
                    float Gain = 0.0f;
                    float AlongVel = 0.0f;
                    float ViewVel = 0.0f;
                    if (InputMag > 0.25f && Speed > CapRef) // real movement input, and over the cap
                    {
                        const FVector VelHN = VelH / Speed; // Speed > CapRef > 0
                        AlongVel = FVector::DotProduct(InputV / InputMag, VelHN);  // input vs motion (ABH ~ +1)
                        const ACharacter* Owner = Self->GetCharacterOwner();
                        FVector ViewFwd = Owner ? Owner->GetControlRotation().Vector() : FVector::ZeroVector;
                        ViewFwd.Z = 0.0f;
                        ViewVel = FVector::DotProduct(ViewFwd.GetSafeNormal(), VelHN); // facing vs motion (ABH ~ -1)
                        // ABH = driving input along your motion (AlongVel high) while facing away (ViewVel<0).
                        if (AlongVel > S.ABHDeadzone && ViewVel < 0.0f)
                        {
                            Gain = FMath::Min((Speed - CapRef) * AlongVel, S.ABHMaxGain);
                            Self->Velocity += Gain * VelHN; // accelerate along motion, horizontal only
                        }
                    }
                    if (S.bDebug)
                    {
                        FHLDebugState& D = FHLDebugState::Get();
                        D.ABHLastBoost = Gain;
                        D.ABHAlongVel = AlongVel;
                        D.ABHViewVel = ViewVel;
                    }
                }
                const float HSpeed = FVector(Self->Velocity.X, Self->Velocity.Y, 0.0f).Size();
                FHLSpeedometerData::Get().RecordJump(HSpeed);
            });

        // Per-frame: slope-landing speed redirect + hold-to-auto-bhop.
        SUBSCRIBE_METHOD_VIRTUAL_AFTER(UFGCharacterMovementComponent::TickComponent, Sample,
            [](UFGCharacterMovementComponent* Self, float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
            {
                FHLMovementSettings& S = FHLMovementSettings::Get();
                if (!S.bEnabled || !HLIsLocalPlayerMovement(Self)) { return; }

                // Surf integration (PM_FlyMove-style, run where the velocity we set survives UE's
                // re-derivation): while airborne and in contact with a non-walkable ramp below us, we
                // own the velocity -- gravity + air-accel + clip-to-ramp -- and assert it here. This is
                // what converts horizontal speed into up-the-ramp speed and lets you climb/launch.
                {
                    static FVector SurfVel = FVector::ZeroVector;
                    bool bSurfingNow = false;
                    if (S.bSurf && Self->MovementMode == MOVE_Falling && Self->UpdatedComponent)
                    {
                        if (const UWorld* World = Self->GetWorld())
                        {
                            const FVector Center = Self->UpdatedComponent->GetComponentLocation();
                            float HalfHeight = 88.0f;
                            if (const UCapsuleComponent* Cap = Cast<UCapsuleComponent>(Self->UpdatedComponent))
                            {
                                HalfHeight = Cap->GetScaledCapsuleHalfHeight();
                            }
                            const FVector End = Center - FVector(0.0f, 0.0f, HalfHeight + 30.0f);
                            FCollisionQueryParams Params(FName(TEXT("HLSurfFloor")), false, Self->GetOwner());
                            FHitResult Hit;
                            if (World->LineTraceSingleByChannel(Hit, Center, End, ECC_Visibility, Params))
                            {
                                const FVector N = Hit.ImpactNormal;
                                if (N.Z > 0.1f && N.Z < Self->GetWalkableFloorZ()) // a ramp: not wall/ceiling, not walkable
                                {
                                    bSurfingNow = true;
                                    if (!GHLWasSurfing) { SurfVel = Self->Velocity; }     // seed on surf entry
                                    SurfVel.Z += Self->GetGravityZ() * DeltaTime;          // gravity (GetGravityZ < 0)
                                    const FVector WishDir = HLComputeWishDir(Self);
                                    const float WishSpeed = Self->GetIsSprinting() ? S.Params.SprintMaxSpeed : S.Params.MaxSpeed;
                                    SurfVel = HLMovementCore::AirAccelerate(SurfVel, WishDir, WishSpeed, S.Params.AirSpeedCap, S.Params.AirAccelerate, DeltaTime);
                                    SurfVel = HLMovementCore::ClipVelocity(SurfVel, N, 1.0f);
                                    Self->Velocity = SurfVel;
                                }
                            }
                        }
                    }
                    if (GHLWasSurfing && !bSurfingNow) { Self->Velocity = SurfVel; } // launch off the ramp carrying the surf velocity
                    GHLWasSurfing = bSurfingNow;

                    if (S.bDebug) { FHLDebugState::Get().bSurfing = bSurfingNow; }
                }

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

                if (S.bDebug)
                {
                    FHLDebugState& D = FHLDebugState::Get();
                    D.MoveMode = Self->MovementMode;
                    D.CustomMode = Self->CustomMovementMode;
                    D.bOnGround = Self->IsMovingOnGround();
                    D.Vel = Self->Velocity;
                }
            });

        // Surf fix (the important one): UE's ComputeSlideVector runs HandleSlopeBoosting while falling,
        // which STRIPS the upward component out of a slide into a ramp (anti-wall-climb). That stops you
        // riding up a surf ramp, and PhysFalling then re-derives velocity from the throttled move and
        // collapses your speed. During surf we override it to the plain plane-projected slide (what Quake
        // PM_FlyMove effectively does), so the character actually climbs the ramp and keeps its speed.
        // ComputeSlideVector is a base UCharacterMovementComponent method (FG does not override it).
        // Reference the PUBLIC base UMovementComponent::ComputeSlideVector; the FG sample resolves the
        // vtable to the actual (protected) UCharacterMovementComponent override at runtime.
        SUBSCRIBE_METHOD_VIRTUAL(UMovementComponent::ComputeSlideVector, Sample,
            [](auto& Scope, const UMovementComponent* Self, const FVector& Delta, float Time,
               const FVector& Normal, const FHitResult& Hit)
            {
                const UCharacterMovementComponent* CMC = Cast<UCharacterMovementComponent>(Self);
                if (CMC == nullptr || !HLIsLocalPlayerMovement(CMC)) { return; }
                FHLMovementSettings& S = FHLMovementSettings::Get();
                if (!S.bEnabled) { return; }
                // Surf any surface too steep to walk on (Normal.Z below the walkable threshold).
                const float WalkableZ = CMC->GetWalkableFloorZ();
                const bool bSteepFalling = (CMC->MovementMode == MOVE_Falling && Normal.Z < WalkableZ);
                if (S.bDebug)
                {
                    FHLDebugState& D = FHLDebugState::Get();
                    D.CSVCalls++; D.CSVNormalZ = Normal.Z; D.CSVWalkableZ = WalkableZ;
                    D.bCSVWouldOverride = (S.bSurf && bSteepFalling);
                }
                if (S.bSurf && bSteepFalling)
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
                if (!S.bEnabled || !HLIsLocalPlayerMovement(Self)) { return; }
                if (S.bDebug)
                {
                    FHLDebugState& D = FHLDebugState::Get();
                    D.SASCalls++; D.SASNormalZ = Normal.Z;
                }
                if (S.bSurf && Self->MovementMode == MOVE_Falling && Normal.Z < Self->GetWalkableFloorZ())
                {
                    // Keep the in-move velocity on the ramp plane; the TickComponent integration is the
                    // authoritative carried velocity (this just keeps the position slide consistent).
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
