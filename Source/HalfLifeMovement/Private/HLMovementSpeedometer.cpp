#include "HLMovementSpeedometer.h"
#include "GameFramework/HUD.h"
#include "Engine/Canvas.h"
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

static float HLConvertUnit(float CmPerSec, int32 Unit)
{
    switch (Unit) { case 1: return CmPerSec / 100.0f; case 2: return CmPerSec * 0.036f; default: return CmPerSec; }
}
static const TCHAR* HLUnitSuffix(int32 Unit)
{
    switch (Unit) { case 1: return TEXT("m/s"); case 2: return TEXT("km/h"); default: return TEXT("u/s"); }
}

// Canvas is protected in AHUD; derive a thin accessor to reach it legally.
struct FHUDCanvasAccessor : public AHUD
{
    static UCanvas* Get(AHUD* HUD) { return static_cast<FHUDCanvasAccessor*>(HUD)->Canvas; }
};

namespace HLSpeedometer
{
    void DrawTo(AHUD* HUD)
    {
        FHLMovementSettings& S = FHLMovementSettings::Get();
        UCanvas* C = HUD ? FHUDCanvasAccessor::Get(HUD) : nullptr;
        if (!S.bEnabled || !S.bSpeedometer || C == nullptr) { return; }

        APawn* P = HUD->GetOwningPawn();
        UCharacterMovementComponent* CMC = P ? P->FindComponentByClass<UCharacterMovementComponent>() : nullptr;
        if (CMC == nullptr) { return; }

        const float HSpeed = FVector(CMC->Velocity.X, CMC->Velocity.Y, 0.0f).Size();
        const float X = C->SizeX * 0.5f - 60.0f;
        float Y = C->SizeY * 0.75f;
        UFont* Font = GEngine->GetMediumFont();

        C->SetDrawColor(255, 255, 255, 255);
        C->DrawText(Font, FString::Printf(TEXT("%.0f %s"), HLConvertUnit(HSpeed, S.SpeedUnit), HLUnitSuffix(S.SpeedUnit)), X, Y);

        if (S.bSpeedometerJumpSpeeds)
        {
            const TArray<FHLJumpRecord>& Hist = FHLSpeedometerData::Get().History();
            for (int32 i = Hist.Num() - 1; i >= 0; --i)
            {
                Y += 18.0f;
                const FHLJumpRecord& R = Hist[i];
                if (R.Delta > 1.0f)       { C->SetDrawColor(80, 255, 80, 255); }   // gained -> green
                else if (R.Delta < -1.0f) { C->SetDrawColor(255, 80, 80, 255); }   // lost -> red
                else                      { C->SetDrawColor(220, 220, 220, 255); }  // ~flat
                C->DrawText(Font, FString::Printf(TEXT("%.0f (%+.0f)"),
                    HLConvertUnit(R.Speed, S.SpeedUnit), HLConvertUnit(R.Delta, S.SpeedUnit)), X, Y);
            }
        }
    }
}
