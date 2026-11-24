#include "HLMovementHooks.h"
#include "HLMovementCore.h"
#include "HLMovementSettings.h"
#include "HLMovementSpeedometer.h"
#include "FGCharacterMovementComponent.h"
#include "FGCharacterBase.h"
#include "FGCharacterPlayer.h"
#include "Equipment/FGParachute.h"
#include "Buildables/FGBuildableConveyorBelt.h"
#include "Patching/NativeHookManager.h"
#include "GameFramework/Character.h"
#include "GameFramework/HUD.h"
#include "FGHUD.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

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
    // Also false when we are a CLIENT connected to someone else's server: the server simulates our
    // character with vanilla movement, so applying HL movement client-side would only fight its
    // corrections (rubber-banding). The mod is RequiredOnRemote=false, so such a client can still
    // join -- it just moves vanilla there. Singleplayer and the host of a listen server are unaffected.
    static bool HLIsLocalPlayerMovement(const UCharacterMovementComponent* Self)
    {
        const ACharacter* Owner = Self ? Self->GetCharacterOwner() : nullptr;
        return Owner != nullptr && Owner->IsPlayerControlled() && Owner->IsLocallyControlled()
            && Owner->GetNetMode() != NM_Client;
    }
}

static FVector HLComputeWishDir(const UFGCharacterMovementComponent* CMC)
{
    FVector Accel = CMC->GetCurrentAcceleration();
    Accel.Z = 0.0f;
    return Accel.GetSafeNormal();
}

// True while the bhop trigger key is held. With bDedicatedBhopKey we poll a configurable key directly
// off the player controller (raw physical key state, independent of the game's input bindings), so the
// bhop trigger is decoupled from the game Jump action that jetpack/parachute are bound to. With the
// toggle off it falls back to FG's mIsPressingJump (the game Jump action) -- the original behavior.
// Local-player gating is done by the callers (HLIsLocalPlayerMovement).
static bool HLBhopHeld(const UFGCharacterMovementComponent* Self, const FHLMovementSettings& S)
{
    if (Self == nullptr) { return false; }
    if (!S.bDedicatedBhopKey) { return Self->mIsPressingJump; }
    const ACharacter* Owner = Self->GetCharacterOwner();
    const APlayerController* PC = Owner ? Cast<APlayerController>(Owner->GetController()) : nullptr;
    if (PC == nullptr) { return false; }
    FKey Key(FName(*S.BhopKey));
    if (!Key.IsValid()) { Key = EKeys::SpaceBar; }
    return PC->IsInputKeyDown(Key);
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

// Returns the local player's parachute equipment iff it is currently DEPLOYED, else nullptr.
// Detection uses the equipment's replicated deployed flag (AFGParachute::IsDeployed); the glide stays
// in CMM_Parachute (so the chute model/anim and FG's deploy state machine are left intact) and the
// returned equipment also gives us its mParachuteSettings to tame. The chute is in the ES_BACK slot.
static AFGParachute* HLGetDeployedParachute(UFGCharacterMovementComponent* Self)
{
    AFGCharacterPlayer* Char = Self ? Cast<AFGCharacterPlayer>(Self->GetCharacterOwner()) : nullptr;
    if (Char == nullptr) { return nullptr; }
    AFGParachute* Chute = Cast<AFGParachute>(Char->GetEquipmentInSlot(EEquipmentSlot::ES_BACK));
    return (Chute != nullptr && Chute->IsDeployed()) ? Chute : nullptr;
}

// Surf: the TickComponent (post-move) hook maintains the player's velocity while in contact with a
// non-walkable ramp -- gravity + air-accel + clip-to-plane, Quake PM_FlyMove style -- and asserts it
// where it survives UE's velocity re-derivation (verified by HL.SurfTestVz). GHLWasSurfing is shared
// so CalcVelocity can skip its own air-accel during surf (the integration owns velocity then).
static bool GHLWasSurfing = false;

// True once the current airborne arc has touched a surf ramp. A surf launch rotates horizontal speed
// into genuine vertical speed (ClipVelocity up the ramp), which the vertical-only fall-damage strip
// cannot remove -- the damaging speed IS the Z. We remember the arc touched surf and let the Landed
// hook consume this flag to exempt the whole landing from fall damage. Set in the surf integration,
// cleared at the next Landed. Singleplayer local player only -> static is fine.
static bool GHLSurfLaunchActive = false;

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

// ABH interval gating: the ABH boost is only earned by a jump spaced far enough from the previous
// one. Going up a ramp, auto-bhop re-jumps with a tiny interval between hops; without this gate every
// one of those rapid hops would take the boost and speed would run away (lethal ramp launch). We
// measure the interval in the DoJump hook directly -- NOT by sampling MovementMode per frame, because
// auto-bhop's own DoJump call synchronously flips MovementMode to MOVE_Falling and corrupts that
// sampling. Singleplayer local player only -> static. Large negative seed so the first jump passes.
static float GHLLastJumpTimeSeconds = -1000.0f;

// Jump footstep (HL.JumpSound): which foot index plays next (alternates 0/1 like HL's step sounds), and
// the min spacing between jump steps so frame-by-frame auto-bhop re-jumps up a ramp don't spam audio.
static int32 GHLJumpSoundFoot = 0;
static constexpr float HLJumpSoundMinInterval = 0.1f;

// Expose AFGCharacterBase::mFallVelocity (protected) so the Landed hook can strip its horizontal
// component for the vertical-only fall-damage fix. A using-declaration re-exposes the protected
// member at public access; we only ever reinterpret an existing character pointer (identical layout,
// no added members/vtable), never instantiate this type.
struct FHLCharFallAccess : public AFGCharacterBase
{
    using AFGCharacterBase::mFallVelocity;
};

// Per-frame tracking used by the TickComponent hook (slope-land boost, surf carry, bhop edge).
static FVector GHLSurfVel = FVector::ZeroVector;
static uint8   GHLLastMode = MOVE_None;
static FVector GHLLastVelocity = FVector::ZeroVector;
static bool    GHLWasBhopHeld = false;

// Parachute HL glide: the parachute settings asset is SHARED, so its originals are captured on chute
// entry and must be put back when the glide ends, the feature or the whole mod is toggled off, or the
// player changes (new world / respawn). Weak pointer: never touch an asset that has been unloaded.
static bool    GHLParaOverride = false;
static FVector GHLParaVelH = FVector::ZeroVector; // our independently-tracked horizontal (Z unused)
static TWeakObjectPtr<UFGParachuteSettings> GHLSavedPS;
static float   GHLSavedStrafe = 0.0f;
static float   GHLSavedShiftStrafe = 0.0f;
static float   GHLSavedLateralConv = 0.0f;
static float   GHLSavedBraking = 0.0f;

static void HLRestoreParachuteSettings()
{
    if (!GHLParaOverride) { return; }
    if (UFGParachuteSettings* PS = GHLSavedPS.Get())
    {
        PS->mStrafeMaxSpeed = GHLSavedStrafe;
        PS->mShiftStrafeMaxSpeed = GHLSavedShiftStrafe;
        PS->mLateralToTerminalConversionRatio = GHLSavedLateralConv;
        PS->mBrakingDeceleration = GHLSavedBraking;
    }
    GHLParaOverride = false;
    GHLSavedPS.Reset();
}

// Engine movement values the CalcVelocity hook overrides on the player's movement component. Captured
// (per component) before the first override so HL.Enabled 0 -- and the per-feature "vanilla" settings
// (HL.WalkableFloorAngle <0, HL.Swim 0) -- can put the real vanilla values back.
struct FHLVanillaMovement
{
    bool  bCaptured = false;
    float WalkableFloorAngle = 0.0f;
    bool  bMaintainHorizontalGroundVelocity = true;
    float MaxSwimSpeed = 0.0f;
};
static FHLVanillaMovement GHLVanilla;

static void HLCaptureVanillaMovement(const UFGCharacterMovementComponent* Self)
{
    if (GHLVanilla.bCaptured) { return; }
    GHLVanilla.WalkableFloorAngle = Self->GetWalkableFloorAngle();
    GHLVanilla.bMaintainHorizontalGroundVelocity = Self->bMaintainHorizontalGroundVelocity;
    GHLVanilla.MaxSwimSpeed = Self->MaxSwimSpeed;
    GHLVanilla.bCaptured = true;
}

static void HLRestoreVanillaMovement(UFGCharacterMovementComponent* Self)
{
    if (!GHLVanilla.bCaptured) { return; }
    Self->SetWalkableFloorAngle(GHLVanilla.WalkableFloorAngle);
    Self->bMaintainHorizontalGroundVelocity = GHLVanilla.bMaintainHorizontalGroundVelocity;
    Self->MaxSwimSpeed = GHLVanilla.MaxSwimSpeed;
    GHLVanilla.bCaptured = false;
}

// All the statics above describe ONE player movement component. When it changes -- a different save
// is loaded, the player respawns, the world is rebuilt -- reset them so nothing stale leaks in. In
// particular world time restarts at 0 on load, so a stale GHLLastJumpTimeSeconds would make every
// JumpInterval negative and silently block ABH and the jump footstep until the clock caught up.
static TWeakObjectPtr<const UCharacterMovementComponent> GHLStateOwner;

static void HLSyncPlayerStateOwner(const UCharacterMovementComponent* Self)
{
    if (GHLStateOwner.Get() == Self) { return; }
    GHLStateOwner = Self;
    HLRestoreParachuteSettings();          // the shared asset must not stay altered
    GHLVanilla = FHLVanillaMovement();     // the new component starts vanilla; capture it fresh
    GHLWasSurfing = false;
    GHLSurfLaunchActive = false;
    GHLApplyDesiredVelocity = false;
    GHLBeltVelocity = FVector::ZeroVector;
    GHLLastJumpTimeSeconds = -1000.0f;
    GHLJumpSoundFoot = 0;
    GHLSurfVel = FVector::ZeroVector;
    GHLLastMode = MOVE_None;
    GHLLastVelocity = FVector::ZeroVector;
    GHLWasBhopHeld = false;
    GHLParaVelH = FVector::ZeroVector;
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
                GHLApplyDesiredVelocity = false; // armed only on the compute paths below
                if (!HLIsLocalPlayerMovement(Self)) { return; }
                HLSyncPlayerStateOwner(Self);
                if (!S.bEnabled) { HLRestoreVanillaMovement(Self); return; }

                // Engine values we override. Capture the vanilla ones first so HL.Enabled 0 (and the
                // per-feature "vanilla" settings below) can put them back instead of leaving them modded.
                HLCaptureVanillaMovement(Self);

                // Velocity follows the ramp surface (false) instead of being flattened to horizontal
                // (true). false => no free speed walking up slopes, and your upward velocity is kept
                // at a ramp crest so you launch into the air when you hit a ramp fast.
                Self->bMaintainHorizontalGroundVelocity = S.bMaintainHorizontalVelocity;

                // Lowering the walkable angle makes steeper ramps non-walkable, so you slide/fall on
                // them (surf) instead of walking up them. <0 = the vanilla angle.
                Self->SetWalkableFloorAngle(S.WalkableFloorAngle >= 0.0f ? S.WalkableFloorAngle : GHLVanilla.WalkableFloorAngle);

                // HL swimming raises MaxSwimSpeed (below); with HL.Swim 0 water is fully vanilla.
                if (!S.bSwim) { Self->MaxSwimSpeed = GHLVanilla.MaxSwimSpeed; }

                if (Self->MovementMode != MOVE_Walking && Self->MovementMode != MOVE_Falling
                    && !(Self->MovementMode == MOVE_Swimming && S.bSwim)) { return; } // defer to vanilla for slide/zipline/HL.Swim 0/etc.

                // Defer to vanilla when an animation is driving movement (emotes/scripted root motion),
                // so we don't override root-motion velocity.
                if (Self->HasAnimRootMotion()) { return; }

                const FVector WishDir = HLComputeWishDir(Self);
                const float WishSpeed = Self->GetIsSprinting() ? S.Params.SprintMaxSpeed : S.Params.MaxSpeed;

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
                    const bool bSkipFriction = S.bAutoHop && HLBhopHeld(Self, S); // about to auto-hop; don't bleed speed
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
                else if (Self->MovementMode == MOVE_Swimming) // only reached with S.bSwim (gated above)
                {
                    // HL PM_WaterMove. Wish direction is the 3D input (GetCurrentAcceleration is NOT
                    // gravity-flattened while swimming, so looking down+forward dives). Bhop key = swim
                    // straight up (ascend); fully idle = drift down. Then WaterMove on the full 3D velocity.
                    const float SwimSpeed = S.Params.MaxSpeed * S.WaterMaxSpeedScale;
                    FVector Wish = Self->GetCurrentAcceleration(); // 3D in water
                    const bool bUp = HLBhopHeld(Self, S);
                    const bool bIdle = Wish.IsNearlyZero() && !bUp;
                    FVector WishVel = Wish.GetSafeNormal() * SwimSpeed;
                    if (bUp) { WishVel.Z = SwimSpeed; }                               // ascend
                    else if (bIdle && S.bWaterSink) { WishVel = FVector(0, 0, -S.WaterSinkSpeed); } // drift down
                    const float SwimWishSpeed = FMath::Min(WishVel.Size(), SwimSpeed);
                    const FVector SwimWishDir = WishVel.GetSafeNormal();
                    NewVel = HLMovementCore::WaterMove(OwnVel, SwimWishDir, SwimWishSpeed,
                        S.WaterAccelerate, S.WaterFriction, Dt);

                    // The engine's PhysSwimming clamps upward velocity to 0.33*MaxSwimSpeed when buoyant,
                    // which would throttle both ascent and the sharking breach. Raise MaxSwimSpeed so the
                    // ceiling sits comfortably above our swim/breach speeds. (Buoyancy stays at its neutral
                    // default so gravity is cancelled underwater; our vertical wish drives up/down instead.)
                    // 3.1 > 1/0.33 (~3.03): engine clamp is 0.33*MaxSwimSpeed, so ceiling stays above both SwimSpeed and WaterBreachZ.
                    Self->MaxSwimSpeed = 3.1f * FMath::Max(SwimSpeed, S.WaterBreachZ);

                    // Sharking: near the surface with the bhop key held, breach -- set an upward velocity
                    // (~a jump) that clears the water. Clearing it flips the engine to MOVE_Falling, where
                    // the existing air-accel + auto-bhop already own movement (no water friction -> emergent
                    // surface skim). Falling back in re-enters this branch and re-breaches: the shark chain.
                    // Gated to near-surface so you can't rocket out of deep water (you must swim up first).
                    if (S.bSharking && bUp && Self->ImmersionDepth() < S.WaterBreachImmersion)
                    {
                        NewVel.Z = FMath::Max(NewVel.Z, S.WaterBreachZ);
                    }
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
                HLSyncPlayerStateOwner(Self);

                // Time since the previous jump. Auto-bhop re-jumps the instant we're grounded, so a
                // real (flat) bhop spaces jumps by its full air arc while machine-gunning up a ramp
                // spaces them by ~a frame. This interval is the reliable discriminator for the ABH
                // boost (per-frame MovementMode sampling is corrupted by DoJump flipping the mode).
                const UWorld* World = Self->GetWorld();
                const float NowSeconds = World ? World->GetTimeSeconds() : 0.0f;
                const float JumpInterval = NowSeconds - GHLLastJumpTimeSeconds;
                GHLLastJumpTimeSeconds = NowSeconds;
                if (S.bDebug) { FHLDebugState::Get().ABHJumpInterval = JumpInterval; }

                // Accelerated Back Hopping (HL2-style, GEOMETRIC trigger). The technique: face AWAY from
                // your travel direction (turn ~180) and bhop -- you gain speed for moving backward relative
                // to where you look. The signal is purely geometric: ViewVel = dot(view, velocity) ~ -1 when
                // you face directly opposite your motion. We deliberately do NOT require a movement key.
                // The old design gated on holding S (input pointing ALONG velocity), but that input also
                // SATURATES the air-accel gate -- holding S pushes dot(velocity, wishDir) over AirSpeedCap so
                // AirAccelerate bails out for EVERY direction, including the perpendicular strafe you need to
                // turn. So you could ABH or steer, never both. Triggering off facing (not input) frees the
                // movement keys: hold A/D + sweep the mouse to TURN while the boost keeps pumping speed,
                // exactly like the HL2 you tested. ViewVel<0 also cleanly separates ABH from a forward (W)
                // bhop, which faces ALONG its motion (ViewVel ~ +1). Boost is ALONG velocity, scaled by how
                // directly you face away (FaceAway = -ViewVel, 0..1) and clamped to a max, so a glancing
                // backward look or surf speeds can't explode it. Before RecordJump so the speedometer reflects it.
                if (S.bABH)
                {
                    const FVector VelH(Self->Velocity.X, Self->Velocity.Y, 0.0f);
                    const float Speed = VelH.Size();
                    const float CapRef = S.Params.MaxSpeed * (1.0f + S.ABHBoost);
                    float Gain = 0.0f;
                    float AlongVel = 0.0f;
                    float ViewVel = 0.0f;
                    if (Speed > CapRef) // over the cap
                    {
                        const FVector VelHN = VelH / Speed; // Speed > CapRef > 0
                        const ACharacter* Owner = Self->GetCharacterOwner();
                        FVector ViewFwd = Owner ? Owner->GetControlRotation().Vector() : FVector::ZeroVector;
                        ViewFwd.Z = 0.0f;
                        ViewVel = FVector::DotProduct(ViewFwd.GetSafeNormal(), VelHN); // facing vs motion (ABH ~ -1)
                        const float FaceAway = -ViewVel; // 0..1, =1 when facing directly opposite your motion
                        // Debug-only: how hard the current input drives along motion (no longer gates the boost).
                        FVector InputV = Self->GetLastInputVector();
                        InputV.Z = 0.0f;
                        const float InputMag = InputV.Size();
                        if (InputMag > KINDA_SMALL_NUMBER) { AlongVel = FVector::DotProduct(InputV / InputMag, VelHN); }
                        // Require this jump to be spaced from the last one. Climbing a ramp, auto-bhop
                        // re-jumps every frame (tiny interval); without this each rapid hop would take
                        // the boost and speed would run away until the ramp launch kills you. A real
                        // flat/downhill bhop has a full air arc between jumps and still boosts every hop.
                        const bool bIntervalOK = JumpInterval >= S.ABHMinInterval;
                        // ABH = facing away from your motion (FaceAway over the deadzone) while over the cap.
                        if (FaceAway > S.ABHDeadzone && bIntervalOK)
                        {
                            Gain = FMath::Min((Speed - CapRef) * FaceAway, S.ABHMaxGain);
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

                // HL1-style takeoff footstep. FG's footsteps come from foot-down anim notifies in the run
                // cycle; a bhop is grounded for 0-1 frames so no notify ever fires and hopping is silent.
                // Play FG's own (surface-aware, Wwise) footstep effect once per jump, alternating feet like
                // HL. Skip near-instant re-jumps (auto-bhop machine-gunning up a ramp re-jumps every frame)
                // so it can't spam a step per frame.
                if (S.bJumpSound && JumpInterval >= HLJumpSoundMinInterval)
                {
                    if (AFGCharacterBase* Char = Cast<AFGCharacterBase>(Self->GetCharacterOwner()))
                    {
                        Char->PlayFootstepEffect(GHLJumpSoundFoot, true);
                        GHLJumpSoundFoot ^= 1;
                    }
                }
            });

        // Vertical-only fall damage. FG's AFGCharacterBase::Landed feeds the MAGNITUDE of its tracked
        // mFallVelocity into CheckFallDamage, so horizontal speed (bhop/ABH) inflates fall damage and a
        // fast landing can kill you even from no real height. Strip the horizontal component of
        // mFallVelocity just before Landed runs (do NOT cancel -- FG still does its landing bookkeeping
        // and applies the now vertical-only damage). This only touches the fall-damage input, not the
        // movement velocity, so bhop speed is preserved on landing. AFGCharacterPlayer doesn't override
        // Landed, so the player's vtable slot is AFGCharacterBase::Landed; the CDO sample resolves it.
        SUBSCRIBE_METHOD_VIRTUAL(AFGCharacterBase::Landed, GetMutableDefault<AFGCharacterPlayer>(),
            [](auto& Scope, AFGCharacterBase* Self, const FHitResult& Hit)
            {
                FHLMovementSettings& S = FHLMovementSettings::Get();
                if (!S.bEnabled || Self == nullptr) { return; }
                if (!HLMovementHooks::HLIsLocalPlayerMovement(Self->GetCharacterMovement())) { return; }
                HLSyncPlayerStateOwner(Self->GetCharacterMovement());
                FVector& FallVel = static_cast<FHLCharFallAccess*>(Self)->mFallVelocity;

                // Surf-launch exemption. A surf launch rotates horizontal speed into vertical (ClipVelocity
                // up the ramp), so the vertical-only strip below can't protect it -- the damaging speed is
                // now in Z. If the airborne arc we're landing from touched a surf ramp, treat it like a
                // bhop: zero the whole fall velocity so the mod-controlled launch never deals fall damage.
                // Consume the flag regardless of the toggle so it can't go stale across landings.
                const bool bWasSurfLaunch = GHLSurfLaunchActive;
                GHLSurfLaunchActive = false;
                if (S.bSurfNoFallDamage && bWasSurfLaunch)
                {
                    FallVel = FVector::ZeroVector;
                    return;
                }

                // Vertical-only fall damage: strip horizontal so fast bhop/ABH landings don't inflate it.
                if (S.bVerticalFallDamageOnly)
                {
                    FallVel.X = 0.0f;
                    FallVel.Y = 0.0f;
                }
            });

        // Per-frame: slope-landing speed redirect + hold-to-auto-bhop.
        SUBSCRIBE_METHOD_VIRTUAL_AFTER(UFGCharacterMovementComponent::TickComponent, Sample,
            [](UFGCharacterMovementComponent* Self, float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
            {
                FHLMovementSettings& S = FHLMovementSettings::Get();
                if (!HLIsLocalPlayerMovement(Self)) { return; }
                HLSyncPlayerStateOwner(Self);
                if (!S.bEnabled)
                {
                    // Mod switched off: put back everything we changed, so "off" really is vanilla.
                    HLRestoreParachuteSettings();
                    HLRestoreVanillaMovement(Self);
                    return;
                }

                // Surf integration (PM_FlyMove-style, run where the velocity we set survives UE's
                // re-derivation): while airborne and in contact with a non-walkable ramp below us, we
                // own the velocity -- gravity + air-accel + clip-to-ramp -- and assert it here. This is
                // what converts horizontal speed into up-the-ramp speed and lets you climb/launch.
                {
                    FVector& SurfVel = GHLSurfVel;
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
                    // Mark the airborne arc as a surf launch so its eventual landing is exempt from fall
                    // damage (the launch turns horizontal speed into vertical, which the vertical-only
                    // strip can't cover). Sticky until the next Landed consumes it.
                    if (bSurfingNow) { GHLSurfLaunchActive = true; }
                    // Safety: once we're back on the ground (and not surfing) the launch arc is over. Landed
                    // normally consumes the flag, but landing in water flips to MOVE_Swimming without firing
                    // Landed, which would leave the flag stale and wrongly exempt a later landing. Clear here
                    // too. (Landed runs inside Super::TickComponent, before this _AFTER hook, so a real
                    // surf-landing has already consumed the flag by the time we get here.)
                    else if (Self->IsMovingOnGround()) { GHLSurfLaunchActive = false; }

                    if (S.bDebug) { FHLDebugState::Get().bSurfing = bSurfingNow; }
                }

                // Only the local player reaches here; last-frame state lives in file-scope statics that
                // HLSyncPlayerStateOwner resets when the player's movement component changes.
                uint8& LastMode = GHLLastMode;
                FVector& LastVelocity = GHLLastVelocity;

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

                // Bhop trigger. The held state comes from HLBhopHeld(): a mod-polled key when
                // HL.DedicatedBhopKey is on (decoupled from the game Jump action / jump-key equipment),
                // else FG's mIsPressingJump. Hold-to-bhop when HL.AutoHop is on; tap-to-jump (rising
                // edge) when it's off. The first DoJump flips MovementMode to falling, so the grounded
                // guard prevents any second trigger the same frame. Local player only -> static is fine.
                bool& bWasBhopHeld = GHLWasBhopHeld;
                const bool bBhopHeld = HLBhopHeld(Self, S);
                const bool bBhopJustPressed = bBhopHeld && !bWasBhopHeld;
                if (Self->IsMovingOnGround() && bBhopHeld && (S.bAutoHop || bBhopJustPressed))
                {
                    Self->DoJump(false);
                }
                bWasBhopHeld = bBhopHeld;

                // Parachute HL glide. We do NOT change the movement mode: forcing MOVE_Falling makes
                // FG's deploy state machine instantly retract the chute (it keeps mIsDeployed synced to
                // CMM_Parachute). Instead we stay in CMM_Parachute (chute stays visible) and tame the
                // parachute from inside its own mode:
                //   (a) Neutralize FG's horizontal handling via the equipment's (public) settings asset
                //       -- huge strafe caps + zero lateral->fall conversion + zero braking -- so
                //       PhysParachute MOVES the player at the real velocity instead of clamping it to
                //       750/1350, bleeding it, or turning speed into fall speed. (Without this, the
                //       per-frame displacement is clamped no matter what velocity we set afterward.)
                //   (b) Own the velocity here, post-move: HL air-accel on X/Y (momentum + air-strafe)
                //       and a downward-only descent cap. This survives into the next PhysParachute the
                //       same way the surf integration's velocity does.
                // The settings asset is shared, so we capture the originals on entry and restore them
                // when the chute closes, the feature or the mod is toggled off, or the player changes
                // (HLRestoreParachuteSettings) -- never leave it altered.
                {
                    FVector& ParaVelH = GHLParaVelH;

                    AFGParachute* Chute = S.bParachuteHLGlide ? HLGetDeployedParachute(Self) : nullptr;
                    UFGParachuteSettings* PS = Chute ? Chute->mParachuteSettings.Get() : nullptr;
                    if (PS != nullptr)
                    {
                        if (!GHLParaOverride || GHLSavedPS.Get() != PS)
                        {
                            HLRestoreParachuteSettings(); // a different asset than before: put the old one back first
                            GHLSavedPS = PS;
                            GHLSavedStrafe = PS->mStrafeMaxSpeed;
                            GHLSavedShiftStrafe = PS->mShiftStrafeMaxSpeed;
                            GHLSavedLateralConv = PS->mLateralToTerminalConversionRatio;
                            GHLSavedBraking = PS->mBrakingDeceleration;
                            ParaVelH = FVector(Self->Velocity.X, Self->Velocity.Y, 0.0f); // seed from incoming momentum
                            GHLParaOverride = true;
                        }
                        PS->mStrafeMaxSpeed = 1.0e6f;
                        PS->mShiftStrafeMaxSpeed = 1.0e6f;
                        PS->mLateralToTerminalConversionRatio = 0.0f;
                        PS->mBrakingDeceleration = 0.0f;

                        // Own horizontal via OUR HL air model on the tracked velocity (discarding FG's
                        // per-frame strafe so a 1e6 cap can't run away); keep PhysParachute's slowed
                        // descent for Z, capped downward to ParachuteFallSpeed.
                        const FVector WishDir = HLComputeWishDir(Self);
                        const float WishSpeed = Self->GetIsSprinting() ? S.Params.SprintMaxSpeed : S.Params.MaxSpeed;
                        const FVector H = HLMovementCore::AirAccelerate(ParaVelH, WishDir, WishSpeed,
                            S.Params.AirSpeedCap, S.Params.AirAccelerate, DeltaTime);
                        ParaVelH = FVector(H.X, H.Y, 0.0f);
                        float Vz = Self->Velocity.Z;
                        if (Vz < -S.ParachuteFallSpeed) { Vz = -S.ParachuteFallSpeed; } // slow descent only
                        Self->Velocity = FVector(ParaVelH.X, ParaVelH.Y, Vz);
                    }
                    else
                    {
                        HLRestoreParachuteSettings(); // chute closed or HL.ParachuteHLGlide 0
                    }
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
                    D.bBhopKeyDown = bBhopHeld;
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
