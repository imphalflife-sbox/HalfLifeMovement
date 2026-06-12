#include "Misc/AutomationTest.h"
#include "HLMovementCore.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHLMoveFrictionTest, "HalfLifeMovement.Core.Friction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHLMoveFrictionTest::RunTest(const FString&)
{
    // Non-zero Z proves friction leaves vertical velocity untouched (the core's central invariant).
    const FVector R = HLMovementCore::ApplyFriction(FVector(400, 0, -250), 4.0f, 100.0f, 0.1f);
    TestEqual(TEXT("friction reduces horizontal speed"), R.X, 240.0);
    TestEqual(TEXT("friction preserves Z"), R.Z, -250.0);
    // Zero horizontal velocity hits the near-stationary early-return (no NaN from div-by-zero).
    const FVector Z = HLMovementCore::ApplyFriction(FVector(0, 0, -250), 4.0f, 100.0f, 0.1f);
    TestEqual(TEXT("friction zeroes tiny horizontal X"), Z.X, 0.0);
    TestEqual(TEXT("friction zeroes tiny horizontal Y"), Z.Y, 0.0);
    TestEqual(TEXT("friction preserves Z when stationary"), Z.Z, -250.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHLMoveAccelTest, "HalfLifeMovement.Core.Accelerate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHLMoveAccelTest::RunTest(const FString&)
{
    FVector R = HLMovementCore::Accelerate(FVector::ZeroVector, FVector(1,0,0), 320.0f, 10.0f, 0.01f);
    TestEqual(TEXT("adds accelspeed along wishdir"), R.X, 32.0);
    R = HLMovementCore::Accelerate(FVector(320,0,0), FVector(1,0,0), 320.0f, 10.0f, 0.01f);
    TestEqual(TEXT("no gain past wishspeed"), R.X, 320.0);
    // Zero wishdir (no input) must be a safe no-op, not a NaN.
    R = HLMovementCore::Accelerate(FVector(200,0,0), FVector::ZeroVector, 0.0f, 10.0f, 0.01f);
    TestEqual(TEXT("zero wishdir is a no-op"), R.X, 200.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHLMoveAirAccelTest, "HalfLifeMovement.Core.AirAccelerate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHLMoveAirAccelTest::RunTest(const FString&)
{
    const float Cap = 30.0f, Accel = 10.0f, Dt = 0.01f;
    FVector R = HLMovementCore::AirAccelerate(FVector(1000,0,0), FVector(0,1,0), 1000.0f, Cap, Accel, Dt);
    TestEqual(TEXT("perpendicular air wish adds speed"), R.Y, 3.0);
    TestTrue(TEXT("speed magnitude grows"), FVector(R.X,R.Y,0).Size() > 1000.0);
    R = HLMovementCore::AirAccelerate(FVector(1000,0,0), FVector(1,0,0), 1000.0f, Cap, Accel, Dt);
    TestEqual(TEXT("forward wish preserves speed"), R.X, 1000.0);
    R = HLMovementCore::AirAccelerate(FVector(1000,0,0), FVector(-1,0,0), 1000.0f, Cap, Accel, Dt);
    TestTrue(TEXT("backward wish changes velocity (ABH)"), R.X < 1000.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHLMoveClipTest, "HalfLifeMovement.Core.ClipVelocity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHLMoveClipTest::RunTest(const FString&)
{
    const FVector N = FVector(-1, 0, 1).GetSafeNormal();
    const FVector R = HLMovementCore::ClipVelocity(FVector(1000, 0, 0), N, 1.0f);
    TestTrue(TEXT("no velocity into plane"), FMath::Abs(FVector::DotProduct(R, N)) < 0.01);
    return true;
}
