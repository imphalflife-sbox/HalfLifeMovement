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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHLMoveSurfAirAccelTest, "HalfLifeMovement.Core.SurfAirAccelerate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHLMoveSurfAirAccelTest::RunTest(const FString&)
{
    const float Cap = 100.0f, Accel = 150.0f, WishSpeed = 600.0f, Dt = 0.01f;
    // Steering against your momentum (sliding off a ramp, strafing back into it): the rate uses the full
    // wish speed, 150*600*0.01 = 900 cm/s this step -- vs 150*100*0.01 = 150 with the capped AirAccelerate.
    FVector R = HLMovementCore::SurfAirAccelerate(FVector(0, -1000, 0), FVector(0, 1, 0), WishSpeed, Cap, Accel, Dt);
    TestEqual(TEXT("opposing: full-rate correction"), R.Y, -100.0, 0.01);
    const FVector Capped = HLMovementCore::AirAccelerate(FVector(0, -1000, 0), FVector(0, 1, 0), WishSpeed, Cap, Accel, Dt);
    TestEqual(TEXT("capped variant is much weaker"), Capped.Y, -850.0, 0.01);
    // Sideways strafe: still limited by the cap (no runaway gain).
    R = HLMovementCore::SurfAirAccelerate(FVector(1000, 0, 0), FVector(0, 1, 0), WishSpeed, Cap, Accel, Dt);
    TestEqual(TEXT("perpendicular: capped at AirSpeedCap"), R.Y, 100.0, 0.01);
    // Already at the cap along the wish: nothing added.
    R = HLMovementCore::SurfAirAccelerate(FVector(0, 100, 0), FVector(0, 1, 0), WishSpeed, Cap, Accel, Dt);
    TestEqual(TEXT("at the cap: no change"), R.Y, 100.0, 0.01);
    // Vertical speed is untouched; no wish is a no-op.
    R = HLMovementCore::SurfAirAccelerate(FVector(0, -1000, -500), FVector(0, 1, 0), WishSpeed, Cap, Accel, Dt);
    TestEqual(TEXT("Z preserved"), R.Z, -500.0, 0.01);
    R = HLMovementCore::SurfAirAccelerate(FVector(0, -1000, 0), FVector::ZeroVector, WishSpeed, Cap, Accel, Dt);
    TestEqual(TEXT("zero wish: no-op"), R.Y, -1000.0, 0.01);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHLMoveWaterTest, "HalfLifeMovement.Core.WaterMove",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHLMoveWaterTest::RunTest(const FString&)
{
    // Proportional friction bleeds the FULL 3D speed (unlike land friction, which preserves Z).
    // No wish (idle): friction only. 400 - 0.1*400*4 = 240, applied to whichever axis carries speed.
    FVector R = HLMovementCore::WaterMove(FVector(400,0,0), FVector::ZeroVector, 0.0f, 10.0f, 4.0f, 0.1f);
    TestEqual(TEXT("water friction bleeds horizontal"), R.X, 240.0);
    R = HLMovementCore::WaterMove(FVector(0,0,-400), FVector::ZeroVector, 0.0f, 10.0f, 4.0f, 0.1f);
    TestEqual(TEXT("water friction bleeds vertical too"), R.Z, -240.0);
    // Vertical accel from rest (swim up): wishdir +Z, wishspeed 200, accelspeed = 10*200*0.01 = 20.
    R = HLMovementCore::WaterMove(FVector::ZeroVector, FVector(0,0,1), 200.0f, 10.0f, 4.0f, 0.01f);
    TestEqual(TEXT("water accel drives wishdir (swim up)"), R.Z, 20.0);
    // Converge: at wishspeed already, friction drops it then accel tops it back to wishspeed.
    R = HLMovementCore::WaterMove(FVector(0,0,200), FVector(0,0,1), 200.0f, 10.0f, 4.0f, 0.01f);
    TestEqual(TEXT("water accel converges to wishspeed"), R.Z, 200.0);
    // Zero wishdir + zero wishspeed is a safe friction-only no-op (no NaN).
    R = HLMovementCore::WaterMove(FVector::ZeroVector, FVector::ZeroVector, 0.0f, 10.0f, 4.0f, 0.01f);
    TestEqual(TEXT("zero velocity stays zero"), R.Size(), 0.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHLMoveClipContactsTest, "HalfLifeMovement.Core.ClipToContactPlanes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHLMoveClipContactsTest::RunTest(const FString&)
{
    // Upward (concave) bend: moving along the flat part (+X) into a steeper face ahead that tilts back
    // (normal leans -X). The redirect must turn the motion UP the face -- this is what surfing up a
    // curved ramp needs, and what the straight-down floor trace could not see.
    const FVector Steep(-0.5f, 0.0f, 0.8660254f);
    FVector R = HLMovementCore::ClipToContactPlanes(FVector(1000, 0, 0), { Steep });
    TestTrue(TEXT("concave bend: redirected upward"), R.Z > 400.0);
    TestTrue(TEXT("concave bend: (750,0,433)"), R.Equals(FVector(750, 0, 433.0127), 0.01));
    TestTrue(TEXT("concave bend: no velocity into the face"), FVector::DotProduct(R, Steep) > -0.01);

    // Moving away from a contact: leave it alone (don't glue the player to the surface).
    const FVector Away(-500, 0, 866);
    R = HLMovementCore::ClipToContactPlanes(Away, { Steep });
    TestTrue(TEXT("moving away is unchanged"), R.Equals(Away, 0.01));

    // V-shaped crease (two faces at once): no plane's single clip works, so slide along the crease.
    const FVector N1(-0.7071068f, 0.0f, 0.7071068f), N2(0.7071068f, 0.0f, 0.7071068f);
    R = HLMovementCore::ClipToContactPlanes(FVector(0, 300, -1000), { N1, N2 });
    TestTrue(TEXT("crease: moves along the crease line"), R.Equals(FVector(0, 300, 0), 0.01));
    TestTrue(TEXT("crease: not into face 1"), FVector::DotProduct(R, N1) > -0.01);
    TestTrue(TEXT("crease: not into face 2"), FVector::DotProduct(R, N2) > -0.01);

    // No contacts: unchanged. Duplicate normals behave like one.
    R = HLMovementCore::ClipToContactPlanes(FVector(1000, 0, 0), {});
    TestTrue(TEXT("no contacts is a no-op"), R.Equals(FVector(1000, 0, 0), 0.01));
    R = HLMovementCore::ClipToContactPlanes(FVector(1000, 0, 0), { Steep, Steep });
    TestTrue(TEXT("duplicate normals act like one"), R.Equals(FVector(750, 0, 433.0127), 0.01));
    return true;
}
