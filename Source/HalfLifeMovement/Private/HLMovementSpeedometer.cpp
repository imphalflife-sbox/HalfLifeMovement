#include "HLMovementSpeedometer.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/Engine.h"
#include "HLMovementSettings.h"

FHLSpeedometerData& FHLSpeedometerData::Get()
{
    static FHLSpeedometerData Instance;
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
}

namespace HLSpeedometer
{
    void DrawTo(AHUD* HUD)
    {
        FHLMovementSettings& S = FHLMovementSettings::Get();
        if (!S.bEnabled || !S.bSpeedometer || HUD == nullptr) { return; }

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
    }
}
