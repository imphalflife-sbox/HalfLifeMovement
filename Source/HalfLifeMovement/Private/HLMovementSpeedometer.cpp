#include "HLMovementSpeedometer.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/Engine.h"
#include "HLMovementSettings.h"
#include "HAL/PlatformTime.h"

FHLSpeedometerData& FHLSpeedometerData::Get()
{
    static FHLSpeedometerData Instance;
    return Instance;
}

FHLDebugState& FHLDebugState::Get()
{
    static FHLDebugState Instance;
    return Instance;
}

FHLJumpKeyWarning& FHLJumpKeyWarning::Get()
{
    static FHLJumpKeyWarning Instance;
    return Instance;
}

void FHLSpeedometerData::RecordJump(float HorizontalSpeed)
{
    FHLJumpRecord R;
    R.Speed = HorizontalSpeed;
    R.Delta = HorizontalSpeed - LastSpeed;
    LastSpeed = HorizontalSpeed;
    Records.Add(R);
    if (Records.Num() > MaxRecords) { Records.RemoveAt(0); }
}

namespace
{
    constexpr float HLLineHeight = 18.0f;

    float HLConvertUnit(float CmPerSec, int32 Unit)
    {
        switch (Unit) { case 1: return CmPerSec / 100.0f; case 2: return CmPerSec * 0.036f; default: return CmPerSec; }
    }
    const TCHAR* HLUnitSuffix(int32 Unit)
    {
        switch (Unit) { case 1: return TEXT("m/s"); case 2: return TEXT("km/h"); default: return TEXT("u/s"); }
    }

    // Two centered lines near the top of the screen: the problem + fix, then how to turn it off.
    // Independent of HL.Speedometer -- the warning matters even with the HUD hidden.
    void HLDrawJumpKeyWarning(AHUD* HUD, const FHLMovementSettings& S)
    {
        const FHLJumpKeyWarning& W = FHLJumpKeyWarning::Get();
        if (!S.bJumpKeyWarning || W.ShowUntil <= 0.0 || FPlatformTime::Seconds() >= W.ShowUntil) { return; }

        APlayerController* PC = HUD->GetOwningPlayerController();
        if (PC == nullptr) { return; }
        int32 SizeX = 0, SizeY = 0;
        PC->GetViewportSize(SizeX, SizeY);
        if (SizeX <= 0 || SizeY <= 0) { return; }

        UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
        const FString Line1 = FString::Printf(
            TEXT("Half-Life Movement: Jump and your bhop key are both %s - the parachute/jetpack will fire on every hop. Rebind Jump in Options -> Controls."),
            *W.KeyName);
        const FString Line2 = TEXT("To stop this warning, enter HL.JumpKeyWarning 0 then HL.Save in the console.");

        const float Scale = 1.15f;
        float Y = SizeY * 0.12f;
        for (const FString* Line : { &Line1, &Line2 })
        {
            float W1 = 0.0f, H1 = 0.0f;
            HUD->GetTextSize(*Line, W1, H1, Font, Scale);
            const FLinearColor Color = (Line == &Line1)
                ? FLinearColor(1.0f, 0.55f, 0.15f, 1.0f)   // FICSIT orange
                : FLinearColor(0.85f, 0.85f, 0.85f, 1.0f);
            HUD->DrawText(*Line, Color, (SizeX - W1) * 0.5f, Y, Font, Scale);
            Y += H1 + 4.0f;
        }
    }
}

namespace HLSpeedometer
{
    void DrawTo(AHUD* HUD)
    {
        FHLMovementSettings& S = FHLMovementSettings::Get();
        if (!S.bEnabled || HUD == nullptr) { return; }
        HLDrawJumpKeyWarning(HUD, S);
        if (!S.bSpeedometer) { return; }

        APlayerController* PC = HUD->GetOwningPlayerController();
        APawn* P = HUD->GetOwningPawn();
        UCharacterMovementComponent* CMC = P ? P->FindComponentByClass<UCharacterMovementComponent>() : nullptr;
        if (PC == nullptr || CMC == nullptr) { return; }

        int32 SizeX = 0, SizeY = 0;
        PC->GetViewportSize(SizeX, SizeY);
        if (SizeX <= 0 || SizeY <= 0) { return; }

        UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr; // AHUD::DrawText tolerates null

        const float HSpeed = FVector(CMC->Velocity.X, CMC->Velocity.Y, 0.0f).Size();
        const float X = SizeX * 0.5f - 60.0f;
        float Y = SizeY * 0.75f;

        HUD->DrawText(FString::Printf(TEXT("%.0f %s"), HLConvertUnit(HSpeed, S.SpeedUnit), HLUnitSuffix(S.SpeedUnit)),
            FLinearColor::White, X, Y, Font);

        if (S.bSpeedometerJumpSpeeds)
        {
            const TArray<FHLJumpRecord>& Hist = FHLSpeedometerData::Get().History();
            for (int32 i = Hist.Num() - 1; i >= 0; --i)
            {
                Y += HLLineHeight;
                const FHLJumpRecord& R = Hist[i];
                FLinearColor Color(0.86f, 0.86f, 0.86f, 1.0f);          // ~flat
                if (R.Delta > 1.0f)       { Color = FLinearColor(0.31f, 1.0f, 0.31f, 1.0f); }  // gained -> green
                else if (R.Delta < -1.0f) { Color = FLinearColor(1.0f, 0.31f, 0.31f, 1.0f); }  // lost -> red
                HUD->DrawText(FString::Printf(TEXT("%.0f (%+.0f)"),
                    HLConvertUnit(R.Speed, S.SpeedUnit), HLConvertUnit(R.Delta, S.SpeedUnit)),
                    Color, X, Y, Font);
            }
        }

        if (S.bDebug)
        {
            const FHLDebugState& D = FHLDebugState::Get();
            const FLinearColor C(1.0f, 1.0f, 0.4f, 1.0f); // yellow
            float DY = SizeY * 0.40f;
            const float DX = 40.0f;
            const TCHAR* ModeName =
                D.MoveMode == 1 ? TEXT("Walk") : D.MoveMode == 3 ? TEXT("Fall") :
                D.MoveMode == 6 ? TEXT("Custom") : TEXT("?");
            HUD->DrawText(FString::Printf(TEXT("mode=%d(%s) custom=%d ground=%d surfing=%d"),
                D.MoveMode, ModeName, D.CustomMode, D.bOnGround ? 1 : 0, D.bSurfing ? 1 : 0), C, DX, DY, Font); DY += HLLineHeight;
            HUD->DrawText(FString::Printf(TEXT("vel H=%.0f Vz=%.0f |v|=%.0f"),
                FVector(D.Vel.X, D.Vel.Y, 0).Size(), D.Vel.Z, D.Vel.Size()), C, DX, DY, Font); DY += HLLineHeight;
            HUD->DrawText(FString::Printf(TEXT("belt=%.0f onBelt=%d"),
                D.BeltSpeed, D.bOnBelt ? 1 : 0), C, DX, DY, Font); DY += HLLineHeight;
            HUD->DrawText(FString::Printf(TEXT("abh=%.0f along=%.2f view=%.2f interval=%.3f"),
                D.ABHLastBoost, D.ABHAlongVel, D.ABHViewVel, D.ABHJumpInterval), C, DX, DY, Font); DY += HLLineHeight;
            HUD->DrawText(FString::Printf(TEXT("bhopKeyDown=%d dedicated=%d key=%s"),
                D.bBhopKeyDown ? 1 : 0, S.bDedicatedBhopKey ? 1 : 0, *S.BhopKey), C, DX, DY, Font); DY += HLLineHeight;
            HUD->DrawText(FString::Printf(TEXT("ComputeSlideVector calls=%u nZ=%.2f walkZ=%.2f override=%d"),
                D.CSVCalls, D.CSVNormalZ, D.CSVWalkableZ, D.bCSVWouldOverride ? 1 : 0), C, DX, DY, Font); DY += HLLineHeight;
            HUD->DrawText(FString::Printf(TEXT("SlideAlongSurface calls=%u nZ=%.2f"),
                D.SASCalls, D.SASNormalZ), C, DX, DY, Font);
        }
    }
}
