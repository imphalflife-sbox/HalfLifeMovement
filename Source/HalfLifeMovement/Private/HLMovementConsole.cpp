#include "HLMovementConsole.h"
#include "HLMovementSettings.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Engine.h"

namespace HLMovementConsole
{
    static void DumpSettings()
    {
        const FHLMovementSettings& S = FHLMovementSettings::Get();
        const FString Line = FString::Printf(
            TEXT("[HL] enabled=%d autohop=%d | accel=%.1f airaccel=%.1f friction=%.1f stopspeed=%.0f airspeedcap=%.0f maxspeed=%.0f sprint=%.0f | parachuteDrag=%.2f speedo=%d unit=%d"),
            S.bEnabled ? 1 : 0, S.bAutoHop ? 1 : 0,
            S.Params.Accelerate, S.Params.AirAccelerate, S.Params.Friction, S.Params.StopSpeed,
            S.Params.AirSpeedCap, S.Params.MaxSpeed, S.Params.SprintMaxSpeed,
            S.ParachuteDragScale, S.bSpeedometer ? 1 : 0, S.SpeedUnit);

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

        static FAutoConsoleVariableRef CVarSpeedo(
            TEXT("HL.Speedometer"), S.bSpeedometer, TEXT("Show the speedometer (0/1)."));
        static FAutoConsoleVariableRef CVarSpeedoJumps(
            TEXT("HL.SpeedometerJumps"), S.bSpeedometerJumpSpeeds, TEXT("Show per-jump speeds on the speedometer (0/1)."));
        static FAutoConsoleVariableRef CVarSpeedUnit(
            TEXT("HL.SpeedUnit"), S.SpeedUnit, TEXT("Speedometer unit: 0=u/s(cm/s), 1=m/s, 2=km/h."));

        static FAutoConsoleCommand CmdDump(
            TEXT("HL.Dump"), TEXT("Print the current Half-Life movement settings."),
            FConsoleCommandDelegate::CreateStatic(&DumpSettings));
    }
}
