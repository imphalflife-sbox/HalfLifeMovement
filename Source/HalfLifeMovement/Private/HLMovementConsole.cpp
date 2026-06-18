#include "HLMovementConsole.h"
#include "HLMovementSettings.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Engine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"

namespace HLMovementConsole
{
    static const TCHAR* HLSection = TEXT("HalfLifeMovement");
    static FString HLConfigFile() { return FPaths::ProjectSavedDir() / TEXT("HalfLifeMovement.ini"); }

    void SaveSettings()
    {
        const FHLMovementSettings& S = FHLMovementSettings::Get();
        const FString File = HLConfigFile();
        GConfig->SetBool (HLSection, TEXT("Enabled"),                  S.bEnabled, File);
        GConfig->SetBool (HLSection, TEXT("AutoHop"),                  S.bAutoHop, File);
        GConfig->SetBool  (HLSection, TEXT("DedicatedBhopKey"),        S.bDedicatedBhopKey, File);
        GConfig->SetString(HLSection, TEXT("BhopKey"),                 *S.BhopKey, File);
        GConfig->SetBool (HLSection, TEXT("MaintainHorizontalVelocity"), S.bMaintainHorizontalVelocity, File);
        GConfig->SetBool (HLSection, TEXT("SlopeLandBoost"),           S.bSlopeLandBoost, File);
        GConfig->SetBool (HLSection, TEXT("Surf"),                     S.bSurf, File);
        GConfig->SetBool (HLSection, TEXT("ConveyorMomentum"),         S.bConveyorMomentum, File);
        GConfig->SetBool (HLSection, TEXT("ABH"),                      S.bABH, File);
        GConfig->SetFloat(HLSection, TEXT("ABHBoost"),                S.ABHBoost, File);
        GConfig->SetFloat(HLSection, TEXT("ABHMaxGain"),              S.ABHMaxGain, File);
        GConfig->SetFloat(HLSection, TEXT("ABHDeadzone"),             S.ABHDeadzone, File);
        GConfig->SetFloat(HLSection, TEXT("ABHMinInterval"),         S.ABHMinInterval, File);
        GConfig->SetBool (HLSection, TEXT("VerticalFallDamageOnly"),  S.bVerticalFallDamageOnly, File);
        GConfig->SetBool (HLSection, TEXT("Swim"),                     S.bSwim, File);
        GConfig->SetBool (HLSection, TEXT("Sharking"),                 S.bSharking, File);
        GConfig->SetFloat(HLSection, TEXT("WaterMaxSpeedScale"),       S.WaterMaxSpeedScale, File);
        GConfig->SetFloat(HLSection, TEXT("WaterAccelerate"),          S.WaterAccelerate, File);
        GConfig->SetFloat(HLSection, TEXT("WaterFriction"),            S.WaterFriction, File);
        GConfig->SetBool (HLSection, TEXT("WaterSink"),                S.bWaterSink, File);
        GConfig->SetFloat(HLSection, TEXT("WaterSinkSpeed"),           S.WaterSinkSpeed, File);
        GConfig->SetFloat(HLSection, TEXT("WaterBreachZ"),             S.WaterBreachZ, File);
        GConfig->SetFloat(HLSection, TEXT("WaterBreachImmersion"),     S.WaterBreachImmersion, File);
        GConfig->SetBool (HLSection, TEXT("CarryParachute"),           S.bCarryParachute, File);
        GConfig->SetBool (HLSection, TEXT("CarryJetpack"),             S.bCarryJetpack, File);
        GConfig->SetFloat(HLSection, TEXT("Accelerate"),              S.Params.Accelerate, File);
        GConfig->SetFloat(HLSection, TEXT("AirAccelerate"),           S.Params.AirAccelerate, File);
        GConfig->SetFloat(HLSection, TEXT("Friction"),               S.Params.Friction, File);
        GConfig->SetFloat(HLSection, TEXT("StopSpeed"),              S.Params.StopSpeed, File);
        GConfig->SetFloat(HLSection, TEXT("AirSpeedCap"),            S.Params.AirSpeedCap, File);
        GConfig->SetFloat(HLSection, TEXT("MaxSpeed"),               S.Params.MaxSpeed, File);
        GConfig->SetFloat(HLSection, TEXT("SprintMaxSpeed"),         S.Params.SprintMaxSpeed, File);
        GConfig->SetFloat(HLSection, TEXT("ParachuteDragScale"),     S.ParachuteDragScale, File);
        GConfig->SetBool (HLSection, TEXT("ParachuteHLGlide"),       S.bParachuteHLGlide, File);
        GConfig->SetFloat(HLSection, TEXT("ParachuteFallSpeed"),     S.ParachuteFallSpeed, File);
        GConfig->SetFloat(HLSection, TEXT("SlopeGravityScale"),      S.SlopeGravityScale, File);
        GConfig->SetFloat(HLSection, TEXT("WalkableFloorAngle"),     S.WalkableFloorAngle, File);
        GConfig->SetBool (HLSection, TEXT("Speedometer"),            S.bSpeedometer, File);
        GConfig->SetBool (HLSection, TEXT("SpeedometerJumpSpeeds"),  S.bSpeedometerJumpSpeeds, File);
        GConfig->SetInt  (HLSection, TEXT("SpeedUnit"),              S.SpeedUnit, File);
        GConfig->Flush(false, File);

        UE_LOG(LogTemp, Display, TEXT("[HL] Settings saved to %s"), *File);
        if (GEngine) { GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, TEXT("[HL] Settings saved.")); }
    }

    void LoadSettings()
    {
        FHLMovementSettings& S = FHLMovementSettings::Get();
        const FString File = HLConfigFile();
        // Each Get* leaves the value untouched (keeps the baked default) if the key isn't present.
        GConfig->GetBool (HLSection, TEXT("Enabled"),                  S.bEnabled, File);
        GConfig->GetBool (HLSection, TEXT("AutoHop"),                  S.bAutoHop, File);
        GConfig->GetBool  (HLSection, TEXT("DedicatedBhopKey"),        S.bDedicatedBhopKey, File);
        GConfig->GetString(HLSection, TEXT("BhopKey"),                 S.BhopKey, File);
        GConfig->GetBool (HLSection, TEXT("MaintainHorizontalVelocity"), S.bMaintainHorizontalVelocity, File);
        GConfig->GetBool (HLSection, TEXT("SlopeLandBoost"),           S.bSlopeLandBoost, File);
        GConfig->GetBool (HLSection, TEXT("Surf"),                     S.bSurf, File);
        GConfig->GetBool (HLSection, TEXT("ConveyorMomentum"),         S.bConveyorMomentum, File);
        GConfig->GetBool (HLSection, TEXT("ABH"),                      S.bABH, File);
        GConfig->GetFloat(HLSection, TEXT("ABHBoost"),                S.ABHBoost, File);
        GConfig->GetFloat(HLSection, TEXT("ABHMaxGain"),              S.ABHMaxGain, File);
        GConfig->GetFloat(HLSection, TEXT("ABHDeadzone"),             S.ABHDeadzone, File);
        GConfig->GetFloat(HLSection, TEXT("ABHMinInterval"),         S.ABHMinInterval, File);
        GConfig->GetBool (HLSection, TEXT("VerticalFallDamageOnly"),  S.bVerticalFallDamageOnly, File);
        GConfig->GetBool (HLSection, TEXT("Swim"),                     S.bSwim, File);
        GConfig->GetBool (HLSection, TEXT("Sharking"),                 S.bSharking, File);
        GConfig->GetFloat(HLSection, TEXT("WaterMaxSpeedScale"),       S.WaterMaxSpeedScale, File);
        GConfig->GetFloat(HLSection, TEXT("WaterAccelerate"),          S.WaterAccelerate, File);
        GConfig->GetFloat(HLSection, TEXT("WaterFriction"),            S.WaterFriction, File);
        GConfig->GetBool (HLSection, TEXT("WaterSink"),                S.bWaterSink, File);
        GConfig->GetFloat(HLSection, TEXT("WaterSinkSpeed"),           S.WaterSinkSpeed, File);
        GConfig->GetFloat(HLSection, TEXT("WaterBreachZ"),             S.WaterBreachZ, File);
        GConfig->GetFloat(HLSection, TEXT("WaterBreachImmersion"),     S.WaterBreachImmersion, File);
        GConfig->GetBool (HLSection, TEXT("CarryParachute"),           S.bCarryParachute, File);
        GConfig->GetBool (HLSection, TEXT("CarryJetpack"),             S.bCarryJetpack, File);
        GConfig->GetFloat(HLSection, TEXT("Accelerate"),              S.Params.Accelerate, File);
        GConfig->GetFloat(HLSection, TEXT("AirAccelerate"),           S.Params.AirAccelerate, File);
        GConfig->GetFloat(HLSection, TEXT("Friction"),               S.Params.Friction, File);
        GConfig->GetFloat(HLSection, TEXT("StopSpeed"),              S.Params.StopSpeed, File);
        GConfig->GetFloat(HLSection, TEXT("AirSpeedCap"),            S.Params.AirSpeedCap, File);
        GConfig->GetFloat(HLSection, TEXT("MaxSpeed"),               S.Params.MaxSpeed, File);
        GConfig->GetFloat(HLSection, TEXT("SprintMaxSpeed"),         S.Params.SprintMaxSpeed, File);
        GConfig->GetFloat(HLSection, TEXT("ParachuteDragScale"),     S.ParachuteDragScale, File);
        GConfig->GetBool (HLSection, TEXT("ParachuteHLGlide"),       S.bParachuteHLGlide, File);
        GConfig->GetFloat(HLSection, TEXT("ParachuteFallSpeed"),     S.ParachuteFallSpeed, File);
        GConfig->GetFloat(HLSection, TEXT("SlopeGravityScale"),      S.SlopeGravityScale, File);
        GConfig->GetFloat(HLSection, TEXT("WalkableFloorAngle"),     S.WalkableFloorAngle, File);
        GConfig->GetBool (HLSection, TEXT("Speedometer"),            S.bSpeedometer, File);
        GConfig->GetBool (HLSection, TEXT("SpeedometerJumpSpeeds"),  S.bSpeedometerJumpSpeeds, File);
        GConfig->GetInt  (HLSection, TEXT("SpeedUnit"),              S.SpeedUnit, File);
    }

    static void DumpSettings()
    {
        const FHLMovementSettings& S = FHLMovementSettings::Get();
        const FString Line = FString::Printf(
            TEXT("[HL] enabled=%d autohop=%d dedBhop=%d bhopKey=%s conveyor=%d abh=%d abhMinInterval=%.2f vertFallDmg=%d | accel=%.1f airaccel=%.1f friction=%.1f stopspeed=%.0f airspeedcap=%.0f maxspeed=%.0f sprint=%.0f | parachuteDrag=%.2f paraGlide=%d paraFall=%.0f speedo=%d unit=%d | swim=%d shark=%d waterScale=%.2f waterAccel=%.1f waterFric=%.1f sink=%d sinkSpd=%.0f breachZ=%.0f breachImm=%.2f"),
            S.bEnabled ? 1 : 0, S.bAutoHop ? 1 : 0, S.bDedicatedBhopKey ? 1 : 0, *S.BhopKey,
            S.bConveyorMomentum ? 1 : 0, S.bABH ? 1 : 0,
            S.ABHMinInterval, S.bVerticalFallDamageOnly ? 1 : 0,
            S.Params.Accelerate, S.Params.AirAccelerate, S.Params.Friction, S.Params.StopSpeed,
            S.Params.AirSpeedCap, S.Params.MaxSpeed, S.Params.SprintMaxSpeed,
            S.ParachuteDragScale, S.bParachuteHLGlide ? 1 : 0, S.ParachuteFallSpeed, S.bSpeedometer ? 1 : 0, S.SpeedUnit,
            S.bSwim ? 1 : 0, S.bSharking ? 1 : 0, S.WaterMaxSpeedScale, S.WaterAccelerate,
            S.WaterFriction, S.bWaterSink ? 1 : 0, S.WaterSinkSpeed, S.WaterBreachZ, S.WaterBreachImmersion);

        UE_LOG(LogTemp, Display, TEXT("%s"), *Line);
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Cyan, Line);
        }
    }

    void RegisterCVars()
    {
        // FAutoConsoleVariableRef binds each console variable directly to the live settings
        // field the movement hooks read, so `HL.MaxSpeed 800` in the console takes effect instantly.
        // Static locals: constructed (and registered) on first call, persist for the process.
        FHLMovementSettings& S = FHLMovementSettings::Get();

        static FAutoConsoleVariableRef CVarEnabled(
            TEXT("HL.Enabled"), S.bEnabled, TEXT("Enable Half-Life movement (0/1)."));
        static FAutoConsoleVariableRef CVarAutoHop(
            TEXT("HL.AutoHop"), S.bAutoHop, TEXT("Hold-jump auto-bhop (0/1)."));
        static FAutoConsoleVariableRef CVarDedicatedBhopKey(
            TEXT("HL.DedicatedBhopKey"), S.bDedicatedBhopKey,
            TEXT("1 = trigger bhop from a mod-polled key (HL.BhopKey) instead of the game Jump action, so jump-key equipment (jetpack/parachute) is never fired by bhopping. Rebind the game Jump off that key in the game menu. 0 = bhop uses the game Jump (original behavior)."));
        static FAutoConsoleVariableRef CVarBhopKey(
            TEXT("HL.BhopKey"), S.BhopKey,
            TEXT("Key name polled for bhop when HL.DedicatedBhopKey=1 (default SpaceBar). e.g. SpaceBar, LeftAlt, LeftControl."));

        static FAutoConsoleVariableRef CVarAccel(
            TEXT("HL.Accelerate"), S.Params.Accelerate, TEXT("Ground acceleration (sv_accelerate)."));
        static FAutoConsoleVariableRef CVarAirAccel(
            TEXT("HL.AirAccelerate"), S.Params.AirAccelerate, TEXT("Air acceleration / airstrafe strength (sv_airaccelerate)."));
        static FAutoConsoleVariableRef CVarFriction(
            TEXT("HL.Friction"), S.Params.Friction, TEXT("Ground friction (sv_friction)."));
        static FAutoConsoleVariableRef CVarStopSpeed(
            TEXT("HL.StopSpeed"), S.Params.StopSpeed, TEXT("Friction low-speed floor (sv_stopspeed)."));
        static FAutoConsoleVariableRef CVarAirCap(
            TEXT("HL.AirSpeedCap"), S.Params.AirSpeedCap, TEXT("Per-tick air wish clamp (airspeedcap)."));
        static FAutoConsoleVariableRef CVarMaxSpeed(
            TEXT("HL.MaxSpeed"), S.Params.MaxSpeed, TEXT("Ground run max speed, cm/s (sv_maxspeed)."));
        static FAutoConsoleVariableRef CVarSprint(
            TEXT("HL.SprintMaxSpeed"), S.Params.SprintMaxSpeed, TEXT("Sprint max speed, cm/s."));
        static FAutoConsoleVariableRef CVarParachuteDrag(
            TEXT("HL.ParachuteDragScale"), S.ParachuteDragScale, TEXT("Parachute horizontal drag scale (lower = glides further)."));
        static FAutoConsoleVariableRef CVarParachuteHLGlide(
            TEXT("HL.ParachuteHLGlide"), S.bParachuteHLGlide,
            TEXT("1 = deployed parachute preserves horizontal velocity + full HL air control, slowing only the vertical descent (to HL.ParachuteFallSpeed); 0 = vanilla FG parachute (clamps horizontal to 750/1350)."));
        static FAutoConsoleVariableRef CVarParachuteFallSpeed(
            TEXT("HL.ParachuteFallSpeed"), S.ParachuteFallSpeed,
            TEXT("Max downward descent speed while HL.ParachuteHLGlide is active, cm/s (default 400). Upward velocity is never capped."));

        static FAutoConsoleVariableRef CVarMaintainHorizVel(
            TEXT("HL.MaintainHorizontalVelocity"), S.bMaintainHorizontalVelocity,
            TEXT("0 = velocity follows ramps (launch off crests, no free up-slope speed); 1 = vanilla flat."));
        static FAutoConsoleVariableRef CVarSlopeLandBoost(
            TEXT("HL.SlopeLandBoost"), S.bSlopeLandBoost,
            TEXT("1 = landing on a slope redirects fall-speed along it (downhill bhop gains speed, uphill loses)."));
        static FAutoConsoleVariableRef CVarSlopeGravity(
            TEXT("HL.SlopeGravity"), S.SlopeGravityScale,
            TEXT("Gravity-along-slope while walking (downhill speed gain). 0 = off; ~1 = full. Causes slow slide on steep ground."));
        static FAutoConsoleVariableRef CVarWalkableAngle(
            TEXT("HL.WalkableFloorAngle"), S.WalkableFloorAngle,
            TEXT("Max walkable slope in degrees. <0 = vanilla. Lower (e.g. 30) makes steeper ramps surfable (slide instead of walk)."));
        static FAutoConsoleVariableRef CVarSurf(
            TEXT("HL.Surf"), S.bSurf,
            TEXT("1 = ride non-walkable ramps while airborne (own velocity: gravity+airaccel+clip); climb/launch up them."));
        static FAutoConsoleVariableRef CVarConveyorMomentum(
            TEXT("HL.ConveyorMomentum"), S.bConveyorMomentum,
            TEXT("1 = ride conveyor belts and keep the speed (additive) when you jump/walk off; 0 = no belt push."));
        static FAutoConsoleVariableRef CVarABH(
            TEXT("HL.ABH"), S.bABH,
            TEXT("1 = HL2 accelerated back hopping: face away from your motion + bhop to gain speed (geometric, no movement key required; hold A/D + sweep the mouse to steer)."));
        static FAutoConsoleVariableRef CVarABHBoost(
            TEXT("HL.ABHBoost"), S.ABHBoost,
            TEXT("Sets the ABH cap: capRef = MaxSpeed*(1+this) (default 0.5). Over capRef, back-hops accelerate."));
        static FAutoConsoleVariableRef CVarABHMaxGain(
            TEXT("HL.ABHMaxGain"), S.ABHMaxGain,
            TEXT("Max horizontal speed a single ABH hop can add, cm/s (default 1000). Bounds surf launches."));
        static FAutoConsoleVariableRef CVarABHDeadzone(
            TEXT("HL.ABHDeadzone"), S.ABHDeadzone,
            TEXT("Min facing-away strength (-dot(view,velocity), 0..1) to engage ABH (default 0.5). Geometric: how directly you must face opposite your motion; no movement key required. Engages when HL.Debug 'view=' drops below -this (e.g. view < -0.5)."));
        static FAutoConsoleVariableRef CVarABHMinInterval(
            TEXT("HL.ABHMinInterval"), S.ABHMinInterval,
            TEXT("Min seconds since the previous jump to earn an ABH boost (default 0.25). Rapid auto-bhop re-jumps up a ramp have a tiny interval and earn no boost; real spaced hops do. Watch 'interval=' on HL.Debug to tune."));
        static FAutoConsoleVariableRef CVarVerticalFallDamage(
            TEXT("HL.VerticalFallDamageOnly"), S.bVerticalFallDamageOnly,
            TEXT("1 = fall damage counts only vertical impact speed (ignore horizontal bhop/ABH speed); 0 = vanilla (full velocity magnitude)."));

        static FAutoConsoleVariableRef CVarSwim(
            TEXT("HL.Swim"), S.bSwim,
            TEXT("1 = HL water movement: swim where you look, bhop key = ascend, idle = sink."));
        static FAutoConsoleVariableRef CVarSharking(
            TEXT("HL.Sharking"), S.bSharking,
            TEXT("1 = sharking: hold the bhop key at the water surface to skip out of the water and skim it fast."));
        static FAutoConsoleVariableRef CVarWaterScale(
            TEXT("HL.WaterMaxSpeedScale"), S.WaterMaxSpeedScale,
            TEXT("Swim speed = MaxSpeed * this (default 0.8, faithful)."));
        static FAutoConsoleVariableRef CVarWaterAccel(
            TEXT("HL.WaterAccelerate"), S.WaterAccelerate, TEXT("Water acceleration (default 10)."));
        static FAutoConsoleVariableRef CVarWaterFriction(
            TEXT("HL.WaterFriction"), S.WaterFriction, TEXT("Proportional water friction, no stop-speed floor (default 4)."));
        static FAutoConsoleVariableRef CVarWaterSink(
            TEXT("HL.WaterSink"), S.bWaterSink, TEXT("1 = drift down when idle in water; 0 = neutral hover."));
        static FAutoConsoleVariableRef CVarWaterSinkSpeed(
            TEXT("HL.WaterSinkSpeed"), S.WaterSinkSpeed, TEXT("Idle sink wish-speed, cm/s (default 100)."));
        static FAutoConsoleVariableRef CVarWaterBreachZ(
            TEXT("HL.WaterBreachZ"), S.WaterBreachZ, TEXT("Upward breach impulse at the surface, cm/s (default 450, ~a jump)."));
        static FAutoConsoleVariableRef CVarWaterBreachImmersion(
            TEXT("HL.WaterBreachImmersion"), S.WaterBreachImmersion,
            TEXT("Breach allowed only when immersion depth < this, 0..1 (default 0.5 = capsule half-submerged)."));

        static FAutoConsoleVariableRef CVarSpeedo(
            TEXT("HL.Speedometer"), S.bSpeedometer, TEXT("Show the speedometer (0/1)."));
        static FAutoConsoleVariableRef CVarSpeedoJumps(
            TEXT("HL.SpeedometerJumps"), S.bSpeedometerJumpSpeeds, TEXT("Show per-jump speeds on the speedometer (0/1)."));
        static FAutoConsoleVariableRef CVarSpeedUnit(
            TEXT("HL.SpeedUnit"), S.SpeedUnit, TEXT("Speedometer unit: 0=u/s(cm/s), 1=m/s, 2=km/h."));
        static FAutoConsoleVariableRef CVarDebug(
            TEXT("HL.Debug"), S.bDebug,
            TEXT("On-screen debug overlay: movement mode, velocity (H/Vz), and hook firing counts."));

        static FAutoConsoleCommand CmdDump(
            TEXT("HL.Dump"), TEXT("Print the current Half-Life movement settings."),
            FConsoleCommandDelegate::CreateStatic(&DumpSettings));
        static FAutoConsoleCommand CmdSave(
            TEXT("HL.Save"), TEXT("Persist the current Half-Life movement settings across restarts."),
            FConsoleCommandDelegate::CreateStatic(&SaveSettings));
    }
}
