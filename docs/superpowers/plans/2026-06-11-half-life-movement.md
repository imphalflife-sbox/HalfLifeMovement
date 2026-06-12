# Half-Life Movement Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A singleplayer Satisfactory C++ mod that replaces base ground/air movement with a GoldSrc-style model (accel + friction, air-strafe, momentum bhop, ABH, auto-bhop, surf), carries momentum into parachute/jetpack, and shows a speedometer — all live-tunable.

**Architecture:** A pure, engine-light "movement core" (Quake math: Friction/Accelerate/AirAccelerate/ClipVelocity) holds the physics and is unit-tested in isolation. Thin SML native hooks on the `final` `UFGCharacterMovementComponent` call into the core for `MOVE_Walking`/`MOVE_Falling` and defer to vanilla otherwise. A settings struct decouples the core/hooks from the (later) SML config asset.

**Tech Stack:** Unreal Engine 5.6.1 (CSS build), C++, Satisfactory Mod Loader (SML) native hooking + config system, Alpakit packaging. Repo: `D:\SatisfactoryModding\SatisfactoryModLoader`.

**Reference spec:** `D:\SatisfactoryModding\docs\superpowers\specs\2026-06-11-half-life-movement-design.md`

---

## File structure

```
Mods/HalfLifeMovement/
  HalfLifeMovement.uplugin
  Resources/Icon128.png                         (human-made; placeholder committed, art later)
  Config/                                        (SML config asset lives here once authored)
  Source/HalfLifeMovement/
    HalfLifeMovement.Build.cs
    Public/
      HLMovementCore.h            FHLMoveParams struct + pure Friction/Accelerate/AirAccelerate/ClipVelocity
      HLMovementSettings.h        FHLMovementSettings (live values) + global accessor
      HLMovementHooks.h           RegisterHooks()/UnregisterHooks()
      HLMovementSpeedometer.h     jump-speed ring buffer + HUD draw
      HalfLifeMovementModule.h    IModuleInterface
    Private/
      HLMovementCore.cpp
      HLMovementSettings.cpp
      HLMovementHooks.cpp
      HLMovementSpeedometer.cpp
      HalfLifeMovementModule.cpp
      Tests/HLMovementCoreTest.cpp   UE automation specs for the core math
```

**Notes for the implementer (read once):**
- The player movement component is `UFGCharacterMovementComponent` (header: `Source/FactoryGame/Public/FGCharacterMovementComponent.h`). It is `final` and extends `UCharacterMovementComponent`. We never subclass it; we hook it.
- SML hooking macros live in `Mods/SML/Source/SML/Public/Patching/NativeHookManager.h`. All our targets are **virtual**, so use `SUBSCRIBE_METHOD_VIRTUAL(Class::Method, SampleInstance, Handler)`. Handler before-signature: `[](auto& Scope, ThisType* Self, Args... Args)`. For `void` methods call `Scope.Cancel()` to skip the original; for value-returning methods call `Scope.Override(Value)` to set the return and skip the original.
- UE units are cm/s. GoldSrc constants assume Quake units (~inches). The math is unit-agnostic; the **constants** in `FHLMoveParams` are starting points and will be tuned in-game.

---

## Phase 1 — Pure movement core (TDD, no engine dependencies beyond FVector)

The core uses only `CoreMinimal` math (`FVector`). All functions operate on the **horizontal plane** (modify X/Y, preserve Z) so vertical/gravity stays vanilla. This phase has no SML/editor dependency and is fully unit-tested.

### Task 1: Define `FHLMoveParams` and the core header

**Files:**
- Create: `Mods/HalfLifeMovement/Source/HalfLifeMovement/Public/HLMovementCore.h`

- [ ] **Step 1: Write the header**

```cpp
#pragma once
#include "CoreMinimal.h"

/** Tunable physics constants (GoldSrc-style). Units are cm/s for speeds. */
struct FHLMoveParams
{
    float Accelerate    = 10.0f;   // sv_accelerate (ground)
    float AirAccelerate = 10.0f;   // sv_airaccelerate
    float Friction      = 4.0f;    // sv_friction
    float StopSpeed     = 100.0f;  // sv_stopspeed
    float AirSpeedCap   = 30.0f;   // airspeedcap (per-tick air wish clamp)
    float MaxSpeed      = 600.0f;  // sv_maxspeed (ground run cap) — TUNE
    float SprintMaxSpeed= 1100.0f; // sprint cap — TUNE
};

/**
 * Pure GoldSrc movement math. All functions take/return a full FVector velocity but
 * only modify the horizontal (X,Y) components; Z is preserved untouched.
 * No engine/world state — directly unit-testable.
 */
namespace HLMovementCore
{
    /** Quake friction. Bleeds horizontal speed toward zero. */
    FVector ApplyFriction(const FVector& Velocity, float Friction, float StopSpeed, float DeltaTime);

    /** Ground acceleration toward WishDir up to WishSpeed. WishDir must be horizontal & unit-length (or zero). */
    FVector Accelerate(const FVector& Velocity, const FVector& WishDir, float WishSpeed, float Accel, float DeltaTime);

    /** Air acceleration with wish-speed clamp (airstrafe / ABH / forward-bhop preserve). */
    FVector AirAccelerate(const FVector& Velocity, const FVector& WishDir, float WishSpeed, float AirSpeedCap, float AirAccel, float DeltaTime);

    /** Clip velocity to a plane (surf/ramp slide). Overbounce 1.0 = pure slide, no speed loss. */
    FVector ClipVelocity(const FVector& Velocity, const FVector& PlaneNormal, float Overbounce);
}
```

- [ ] **Step 2: Commit**

```bash
cd /d/SatisfactoryModding/SatisfactoryModLoader
git add Mods/HalfLifeMovement/Source/HalfLifeMovement/Public/HLMovementCore.h
git commit -m "feat(hlmove): add movement core header and params struct"
```

### Task 2: `ApplyFriction` (TDD)

**Files:**
- Test: `Mods/HalfLifeMovement/Source/HalfLifeMovement/Private/Tests/HLMovementCoreTest.cpp`
- Modify: `Mods/HalfLifeMovement/Source/HalfLifeMovement/Private/HLMovementCore.cpp` (create)

- [ ] **Step 1: Write the failing test**

```cpp
#include "Misc/AutomationTest.h"
#include "HLMovementCore.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHLMoveFrictionTest, "HalfLifeMovement.Core.Friction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHLMoveFrictionTest::RunTest(const FString&)
{
    // Above stopspeed: drop = speed * friction * dt = 400 * 4 * 0.1 = 160 -> 240 left
    const FVector Result = HLMovementCore::ApplyFriction(FVector(400, 0, 0), 4.0f, 100.0f, 0.1f);
    TestEqual(TEXT("friction reduces horizontal speed"), Result.X, 240.0f);
    TestEqual(TEXT("friction preserves Z"), Result.Z, 0.0f);
    return true;
}
```

- [ ] **Step 2: Run test, verify it fails** — build the editor target, then:

Run: `"C:\Program Files\Unreal Engine - CSS\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\SatisfactoryModding\SatisfactoryModLoader\FactoryGame.uproject" -ExecCmds="Automation RunTests HalfLifeMovement.Core.Friction; Quit" -unattended -nopause -nullrhi -log`
Expected: FAIL — `ApplyFriction` unresolved / returns wrong value.

- [ ] **Step 3: Implement `ApplyFriction`**

```cpp
#include "HLMovementCore.h"

namespace HLMovementCore
{
    FVector ApplyFriction(const FVector& Velocity, float Friction, float StopSpeed, float DeltaTime)
    {
        FVector V = Velocity;
        const float Speed = FVector(V.X, V.Y, 0.0f).Size();
        if (Speed < KINDA_SMALL_NUMBER)
        {
            V.X = 0.0f; V.Y = 0.0f;
            return V;
        }
        const float Control = Speed < StopSpeed ? StopSpeed : Speed;
        const float Drop = Control * Friction * DeltaTime;
        const float NewSpeed = FMath::Max(Speed - Drop, 0.0f) / Speed;
        V.X *= NewSpeed; V.Y *= NewSpeed;
        return V;
    }
}
```

- [ ] **Step 4: Run test, verify it passes** (same command as Step 2). Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add Mods/HalfLifeMovement/Source/HalfLifeMovement/Private/HLMovementCore.cpp Mods/HalfLifeMovement/Source/HalfLifeMovement/Private/Tests/HLMovementCoreTest.cpp
git commit -m "feat(hlmove): implement ApplyFriction with test"
```

### Task 3: `Accelerate` (TDD)

**Files:**
- Modify: `.../Private/Tests/HLMovementCoreTest.cpp`, `.../Private/HLMovementCore.cpp`

- [ ] **Step 1: Add failing tests**

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHLMoveAccelTest, "HalfLifeMovement.Core.Accelerate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHLMoveAccelTest::RunTest(const FString&)
{
    // From rest, wish forward: addspeed=320, accelspeed=min(10*320*0.01,320)=32 -> X=32
    FVector R = HLMovementCore::Accelerate(FVector::ZeroVector, FVector(1,0,0), 320.0f, 10.0f, 0.01f);
    TestEqual(TEXT("adds accelspeed along wishdir"), R.X, 32.0f);

    // Already at/above wishspeed in wishdir: addspeed<=0 -> unchanged
    R = HLMovementCore::Accelerate(FVector(320,0,0), FVector(1,0,0), 320.0f, 10.0f, 0.01f);
    TestEqual(TEXT("no gain past wishspeed"), R.X, 320.0f);
    return true;
}
```

- [ ] **Step 2: Run, verify FAIL** (command from Task 2 Step 2, swap test name to `HalfLifeMovement.Core.Accelerate`).

- [ ] **Step 3: Implement**

```cpp
    FVector Accelerate(const FVector& Velocity, const FVector& WishDir, float WishSpeed, float Accel, float DeltaTime)
    {
        const FVector Dir(WishDir.X, WishDir.Y, 0.0f);
        const float CurrentSpeed = FVector::DotProduct(FVector(Velocity.X, Velocity.Y, 0.0f), Dir);
        const float AddSpeed = WishSpeed - CurrentSpeed;
        if (AddSpeed <= 0.0f) { return Velocity; }
        float AccelSpeed = Accel * WishSpeed * DeltaTime;
        if (AccelSpeed > AddSpeed) { AccelSpeed = AddSpeed; }
        FVector V = Velocity;
        V.X += AccelSpeed * Dir.X;
        V.Y += AccelSpeed * Dir.Y;
        return V;
    }
```

- [ ] **Step 4: Run, verify PASS.**
- [ ] **Step 5: Commit** — `git commit -am "feat(hlmove): implement Accelerate with test"`

### Task 4: `AirAccelerate` (TDD — the heart of airstrafe/ABH)

**Files:** same two test/impl files.

- [ ] **Step 1: Add failing tests** (airstrafe gain, forward-bhop preserve, ABH gain)

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHLMoveAirAccelTest, "HalfLifeMovement.Core.AirAccelerate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHLMoveAirAccelTest::RunTest(const FString&)
{
    const float Cap = 30.0f, Accel = 10.0f, Dt = 0.01f;

    // Airstrafe: moving +X fast, wish +Y (perpendicular). dot~0 -> add full capped along Y.
    // accelspeed = min(10*30*0.01, 30-0) = 3 -> Y becomes 3, |v| grows.
    FVector R = HLMovementCore::AirAccelerate(FVector(1000,0,0), FVector(0,1,0), 1000.0f, Cap, Accel, Dt);
    TestEqual(TEXT("perpendicular air wish adds speed"), R.Y, 3.0f);
    TestTrue(TEXT("speed magnitude grows"), FVector(R.X,R.Y,0).Size() > 1000.0f);

    // Forward bhop: moving +X fast, wish +X. dot=1000 >= cap -> addspeed<=0 -> unchanged.
    R = HLMovementCore::AirAccelerate(FVector(1000,0,0), FVector(1,0,0), 1000.0f, Cap, Accel, Dt);
    TestEqual(TEXT("forward wish preserves speed"), R.X, 1000.0f);

    // ABH: moving +X, wish -X (hold S facing forward / back-hop). dot=-1000, addspeed=cap+1000>0 -> adds along -X.
    R = HLMovementCore::AirAccelerate(FVector(1000,0,0), FVector(-1,0,0), 1000.0f, Cap, Accel, Dt);
    TestTrue(TEXT("backward wish changes velocity (ABH term active)"), R.X < 1000.0f);
    return true;
}
```

- [ ] **Step 2: Run, verify FAIL.**

- [ ] **Step 3: Implement**

```cpp
    FVector AirAccelerate(const FVector& Velocity, const FVector& WishDir, float WishSpeed, float AirSpeedCap, float AirAccel, float DeltaTime)
    {
        const FVector Dir(WishDir.X, WishDir.Y, 0.0f);
        const float WishSpd = FMath::Min(WishSpeed, AirSpeedCap);
        const float CurrentSpeed = FVector::DotProduct(FVector(Velocity.X, Velocity.Y, 0.0f), Dir);
        const float AddSpeed = WishSpd - CurrentSpeed;
        if (AddSpeed <= 0.0f) { return Velocity; }
        float AccelSpeed = AirAccel * WishSpd * DeltaTime;
        if (AccelSpeed > AddSpeed) { AccelSpeed = AddSpeed; }
        FVector V = Velocity;
        V.X += AccelSpeed * Dir.X;
        V.Y += AccelSpeed * Dir.Y;
        return V;
    }
```

- [ ] **Step 4: Run, verify PASS.**
- [ ] **Step 5: Commit** — `git commit -am "feat(hlmove): implement AirAccelerate (airstrafe/ABH) with tests"`

### Task 5: `ClipVelocity` (TDD — surf/ramp)

- [ ] **Step 1: Add failing test**

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHLMoveClipTest, "HalfLifeMovement.Core.ClipVelocity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHLMoveClipTest::RunTest(const FString&)
{
    // Velocity into a 45-degree ramp; clipping removes the into-plane component, slide preserved.
    const FVector N = FVector(-1, 0, 1).GetSafeNormal();
    const FVector R = HLMovementCore::ClipVelocity(FVector(1000, 0, 0), N, 1.0f);
    // Result must have no component into the plane:
    TestTrue(TEXT("no velocity into plane"), FMath::Abs(FVector::DotProduct(R, N)) < 0.01f);
    return true;
}
```

- [ ] **Step 2: Run, verify FAIL.**

- [ ] **Step 3: Implement**

```cpp
    FVector ClipVelocity(const FVector& Velocity, const FVector& PlaneNormal, float Overbounce)
    {
        const float Backoff = FVector::DotProduct(Velocity, PlaneNormal) * Overbounce;
        return Velocity - PlaneNormal * Backoff;
    }
```

- [ ] **Step 4: Run, verify PASS.**
- [ ] **Step 5: Commit** — `git commit -am "feat(hlmove): implement ClipVelocity (surf/ramp) with test"`

---

## Phase 2 — Mod scaffold & hook spike (editor + SML API verification)

This phase is editor-interactive and de-risks the SML hook syntax before heavy logic. **Do these steps in the open Unreal editor**; commands that build are given explicitly.

### Task 6: Scaffold the mod plugin via the SML wizard

**Files:** generates `Mods/HalfLifeMovement/HalfLifeMovement.uplugin`, `Source/HalfLifeMovement/HalfLifeMovement.Build.cs`, module `.h/.cpp`.

- [ ] **Step 1:** Open the project: launch `UnrealEditor.exe` on `FactoryGame.uproject` (or via the `.sln`).
- [ ] **Step 2:** Use the SML mod wizard: editor menu **Modding → Create Mod** (a.k.a. the Alpakit "Create Mod" entry). Set **Mod Reference = `HalfLifeMovement`**, display name "Half-Life Movement", a C++ mod (not Blueprint-only).
- [ ] **Step 3:** Close the editor. Confirm the generated files exist:

Run: `ls "D:\SatisfactoryModding\SatisfactoryModLoader\Mods\HalfLifeMovement\Source\HalfLifeMovement"`
Expected: a `.Build.cs`, module `.h`/`.cpp`, and `Mods/HalfLifeMovement/HalfLifeMovement.uplugin` present.

- [ ] **Step 4:** Edit `HalfLifeMovement.Build.cs` `PublicDependencyModuleNames` to include the modules we need:

```csharp
PublicDependencyModuleNames.AddRange(new string[] {
    "Core", "CoreUObject", "Engine", "InputCore",
    "FactoryGame", "SML"
});
```

- [ ] **Step 5: Commit**

```bash
git add Mods/HalfLifeMovement
git commit -m "chore(hlmove): scaffold HalfLifeMovement SML mod plugin"
```

### Task 7: Hook spike — prove `GetMaxSpeed` override compiles & runs

This validates the exact `SUBSCRIBE_METHOD_VIRTUAL` + `CallScope` syntax against the live SML before we build real logic.

**Files:**
- Create: `.../Public/HLMovementHooks.h`, `.../Private/HLMovementHooks.cpp`
- Modify: the generated `HalfLifeMovementModule.cpp`

- [ ] **Step 1: Write the hooks header**

```cpp
#pragma once
namespace HLMovementHooks { void RegisterHooks(); }
```

- [ ] **Step 2: Write a minimal spike hook**

```cpp
#include "HLMovementHooks.h"
#include "FGCharacterMovementComponent.h"
#include "Patching/NativeHookManager.h"

namespace HLMovementHooks
{
    void RegisterHooks()
    {
        UFGCharacterMovementComponent* Sample = GetMutableDefault<UFGCharacterMovementComponent>();

        // SPIKE: while airborne, report a huge max speed so UE's air clamp can't throttle us.
        SUBSCRIBE_METHOD_VIRTUAL(UFGCharacterMovementComponent::GetMaxSpeed, Sample,
            [](auto& Scope, const UFGCharacterMovementComponent* Self)
            {
                if (Self && Self->MovementMode == MOVE_Falling)
                {
                    Scope.Override(1000000.0f);
                }
            });
    }
}
```

- [ ] **Step 3: Call it from module startup** — in `HalfLifeMovementModule.cpp`:

```cpp
#include "HLMovementHooks.h"
// inside StartupModule():
HLMovementHooks::RegisterHooks();
```

- [ ] **Step 4: Build the editor target**

Run: `"C:\Program Files\Unreal Engine - CSS\Engine\Build\BatchFiles\Build.bat" FactoryEditor Win64 Development -project="D:\SatisfactoryModding\SatisfactoryModLoader\FactoryGame.uproject" -waitmutex`
Expected: `Result: Succeeded`. (If `SUBSCRIBE_METHOD_VIRTUAL`/`Scope.Override` mismatch, fix syntax now — this is the spike's purpose.)

- [ ] **Step 5: Manually verify in PIE** — open editor, Play, jump: no errors; jumping no longer clamps horizontal speed mid-air (you keep sliding). Confirm via console `stat` or the speedometer added later. Acceptable interim check: no crash, log line printed.
- [ ] **Step 6: Commit** — `git add -A && git commit -m "feat(hlmove): SML hook spike — GetMaxSpeed air override compiles & runs"`

---

## Phase 3 — Settings struct + wire core into walking/falling

### Task 8: `FHLMovementSettings` live-values struct + accessor

**Files:** Create `.../Public/HLMovementSettings.h`, `.../Private/HLMovementSettings.cpp`

- [ ] **Step 1: Header** — a single global holder the hooks read and (later) the config writes:

```cpp
#pragma once
#include "CoreMinimal.h"
#include "HLMovementCore.h"

struct FHLMovementSettings
{
    bool  bEnabled = true;
    bool  bAutoHop = true;
    bool  bCarryParachute = true;
    bool  bCarryJetpack = true;
    float ParachuteDragScale = 0.25f;
    FHLMoveParams Params;            // accel/airaccel/friction/cap/maxspeed/...
    // Speedometer
    bool  bSpeedometer = true;
    bool  bSpeedometerJumpSpeeds = true;
    int32 SpeedUnit = 0;             // 0=u/s(cm/s) 1=m/s 2=km/h

    static FHLMovementSettings& Get();
};
```

- [ ] **Step 2: Impl**

```cpp
#include "HLMovementSettings.h"
FHLMovementSettings& FHLMovementSettings::Get()
{
    static FHLMovementSettings Instance;
    return Instance;
}
```

- [ ] **Step 3: Commit** — `git add -A && git commit -m "feat(hlmove): add live settings struct"`

### Task 9: Helper — build horizontal WishDir from input + view

**Files:** Modify `.../Private/HLMovementHooks.cpp` (add a file-local helper)

- [ ] **Step 1: Add helper** (uses the pawn's control rotation yaw and the CMC's pending input)

```cpp
// Returns a horizontal unit wish direction from the component's current acceleration input,
// flattened to the XY plane. Acceleration already encodes WASD relative to control rotation.
static FVector HLComputeWishDir(const UFGCharacterMovementComponent* CMC)
{
    FVector Accel = CMC->GetCurrentAcceleration();
    Accel.Z = 0.0f;
    return Accel.GetSafeNormal();
}
```

> Note: `GetCurrentAcceleration()` returns the world-space desired direction derived from input + control rotation, which is exactly the GoldSrc `wishdir`. This avoids manually reconstructing yaw.

- [ ] **Step 2: Commit** — `git commit -am "feat(hlmove): add wishdir helper from input acceleration"`

### Task 10: Override `CalcVelocity` for walking & falling

**Files:** Modify `.../Private/HLMovementHooks.cpp`

- [ ] **Step 1: Add the hook in `RegisterHooks()`** (after the GetMaxSpeed hook)

```cpp
SUBSCRIBE_METHOD_VIRTUAL(UFGCharacterMovementComponent::CalcVelocity, Sample,
    [](auto& Scope, UFGCharacterMovementComponent* Self, float Dt, float Friction, bool bFluid, float BrakingDecel)
    {
        FHLMovementSettings& S = FHLMovementSettings::Get();
        if (!S.bEnabled || Self == nullptr) { return; }              // vanilla
        const uint8 Mode = Self->MovementMode;
        if (Mode != MOVE_Walking && Mode != MOVE_Falling) { return; } // defer (slide/zipline/etc.)

        const FVector WishDir = HLComputeWishDir(Self);
        const bool bSprint = Self->GetIsSprinting();
        const float WishSpeed = bSprint ? S.Params.SprintMaxSpeed : S.Params.MaxSpeed;

        if (Mode == MOVE_Walking)
        {
            FVector V = Self->Velocity;
            V = HLMovementCore::ApplyFriction(V, S.Params.Friction, S.Params.StopSpeed, Dt);
            V = HLMovementCore::Accelerate(V, WishDir, WishSpeed, S.Params.Accelerate, Dt);
            Self->Velocity = V;
        }
        else // MOVE_Falling — horizontal only; gravity handled by vanilla PhysFalling
        {
            Self->Velocity = HLMovementCore::AirAccelerate(
                Self->Velocity, WishDir, WishSpeed, S.Params.AirSpeedCap, S.Params.AirAccelerate, Dt);
        }
        Scope.Cancel();   // skip vanilla CalcVelocity for these modes
    });
```

- [ ] **Step 2: Add includes** at top of file: `#include "HLMovementCore.h"` and `#include "HLMovementSettings.h"`.

- [ ] **Step 3: Build** — Build.bat FactoryEditor (command from Task 7 Step 4). Expected: `Result: Succeeded`.

- [ ] **Step 4: Manual verify (PIE)** — movement should feel different: quicker stops (friction), and in the air you can airstrafe (hold A/D + turn) to curve and gain speed. Use the temporary log or wait for Phase 5 speedometer for numbers. Confirm: no crash; ground movement still works; entering a hypertube/zipline still behaves vanilla (those are other modes).

- [ ] **Step 5: Commit** — `git commit -am "feat(hlmove): override CalcVelocity for walk/air with GoldSrc model"`

### Task 11: Make `GetMaxSpeed` respect the enable toggle & ground cap

**Files:** Modify the GetMaxSpeed hook from Task 7.

- [ ] **Step 1: Replace the spike hook body**

```cpp
SUBSCRIBE_METHOD_VIRTUAL(UFGCharacterMovementComponent::GetMaxSpeed, Sample,
    [](auto& Scope, const UFGCharacterMovementComponent* Self)
    {
        FHLMovementSettings& S = FHLMovementSettings::Get();
        if (!S.bEnabled || Self == nullptr) { return; }
        if (Self->MovementMode == MOVE_Falling)
        {
            Scope.Override(1000000.0f);     // never clamp air speed (only AirAccelerate governs it)
        }
        // Walking: leave vanilla GetMaxSpeed; our Accelerate already caps to WishSpeed.
    });
```

- [ ] **Step 2: Build → Succeeded. Step 3: PIE — forward bhop maintains speed, airstrafe builds it.**
- [ ] **Step 4: Commit** — `git commit -am "feat(hlmove): gate GetMaxSpeed air-uncap behind enable toggle"`

---

## Phase 4 — Jump: momentum bhop, auto-hop, takeoff-speed capture

### Task 12: Capture takeoff speed + preserve momentum in `DoJump`

**Files:** Modify `.../Private/HLMovementHooks.cpp`; Create `.../Public/HLMovementSpeedometer.h` (ring buffer only for now).

- [ ] **Step 1: Speedometer header (data side)**

```cpp
#pragma once
#include "CoreMinimal.h"

struct FHLJumpRecord { float Speed = 0.0f; float Delta = 0.0f; };

class FHLSpeedometerData
{
public:
    static FHLSpeedometerData& Get();
    void RecordJump(float HorizontalSpeed);      // pushes speed + delta vs previous
    const TArray<FHLJumpRecord>& History() const { return Records; }
private:
    TArray<FHLJumpRecord> Records;               // most recent at end, capped
    float LastSpeed = 0.0f;
    static constexpr int32 MaxRecords = 8;
};
```

- [ ] **Step 2: Speedometer data impl** — Create `.../Private/HLMovementSpeedometer.cpp`

```cpp
#include "HLMovementSpeedometer.h"

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
```

- [ ] **Step 3: Hook `DoJump`** in `RegisterHooks()` (after CalcVelocity). `DoJump` returns `bool`; we don't override the result, we only observe (use `_AFTER` so the jump already happened, then record the resulting horizontal speed):

```cpp
SUBSCRIBE_METHOD_VIRTUAL_AFTER(UFGCharacterMovementComponent::DoJump, Sample,
    [](auto& Scope, UFGCharacterMovementComponent* Self, bool bReplayingMoves)
    {
        if (!FHLMovementSettings::Get().bEnabled || Self == nullptr) { return; }
        const float HSpeed = FVector(Self->Velocity.X, Self->Velocity.Y, 0.0f).Size();
        FHLSpeedometerData::Get().RecordJump(HSpeed);
    });
```

> `SUBSCRIBE_METHOD_VIRTUAL_AFTER` handler signature is `(auto& Scope, ThisType* Self, Args...)` like the before form; the original has already run. Momentum is preserved automatically because vanilla `DoJump` only sets `Velocity.Z` and never zeroes horizontal — confirm in PIE.

- [ ] **Step 4: Add include** `#include "HLMovementSpeedometer.h"`.
- [ ] **Step 5: Build → Succeeded.**
- [ ] **Step 6: Commit** — `git commit -am "feat(hlmove): record per-jump takeoff speed on DoJump"`

### Task 13: Hold-to-auto-bhop (pre-empt landing friction)

The friction-ordering requirement: on landing, if jump is held, re-jump before the walking friction tick. We implement by hooking the landing transition and re-arming the jump.

**Files:** Modify `.../Private/HLMovementHooks.cpp`

- [ ] **Step 1: Hook `OnMovementModeChanged`** to detect Falling→Walking and auto-jump

```cpp
SUBSCRIBE_METHOD_VIRTUAL_AFTER(UFGCharacterMovementComponent::OnMovementModeChanged, Sample,
    [](auto& Scope, UFGCharacterMovementComponent* Self, EMovementMode PrevMode, uint8 PrevCustom)
    {
        FHLMovementSettings& S = FHLMovementSettings::Get();
        if (!S.bEnabled || !S.bAutoHop || Self == nullptr) { return; }
        // Just landed (now walking, was falling) and the player is holding jump:
        ACharacter* Ch = Cast<ACharacter>(Self->GetCharacterOwner());
        if (Self->MovementMode == MOVE_Walking && PrevMode == MOVE_Falling && Ch && Ch->bPressedJump)
        {
            // Re-jump immediately, before friction is applied this frame.
            Self->DoJump(false);
        }
    });
```

> Why this works: `OnMovementModeChanged` fires when the move is processed; jumping here happens before the walking `CalcVelocity`/friction for the settled frame. Verify on the speedometer that chained hops preserve speed (delta ~0 on straight hops, positive when airstrafing). If friction still bleeds, the fallback is to additionally skip friction in the Task 10 walking branch when `bPressedJump && bAutoHop` for the landing frame — add:
> ```cpp
> // inside MOVE_Walking branch, before ApplyFriction:
> const ACharacter* Ch = Cast<ACharacter>(Self->GetCharacterOwner());
> const bool bSkipFriction = S.bAutoHop && Ch && Ch->bPressedJump;
> if (!bSkipFriction) { V = HLMovementCore::ApplyFriction(V, S.Params.Friction, S.Params.StopSpeed, Dt); }
> ```

- [ ] **Step 2: Build → Succeeded.**
- [ ] **Step 3: Manual verify (PIE):** hold jump → continuous frame-perfect hops; speed is preserved across straight hops and grows when airstrafing/ABH. If speed decays on straight hops, apply the friction-skip fallback above and rebuild.
- [ ] **Step 4: Commit** — `git commit -am "feat(hlmove): hold-to-auto-bhop pre-empting landing friction"`

---

## Phase 5 — Speedometer HUD

### Task 14: Draw current speed + color-coded jump history

**Files:** Modify `.../Public/HLMovementSpeedometer.h`, `.../Private/HLMovementSpeedometer.cpp`; hook the HUD.

- [ ] **Step 1: Add a draw entry point** to the header

```cpp
class AHUD;
namespace HLSpeedometer { void DrawTo(AHUD* HUD); }
```

- [ ] **Step 2: Implement the draw** using the HUD canvas

```cpp
#include "GameFramework/HUD.h"
#include "Engine/Canvas.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "FGCharacterMovementComponent.h"
#include "HLMovementSettings.h"

static float HLConvertUnit(float CmPerSec, int32 Unit)
{
    switch (Unit) { case 1: return CmPerSec / 100.0f; case 2: return CmPerSec * 0.036f; default: return CmPerSec; }
}
static const TCHAR* HLUnitSuffix(int32 Unit){ switch(Unit){case 1:return TEXT("m/s");case 2:return TEXT("km/h");default:return TEXT("u/s");} }

namespace HLSpeedometer
{
    void DrawTo(AHUD* HUD)
    {
        FHLMovementSettings& S = FHLMovementSettings::Get();
        if (!S.bEnabled || !S.bSpeedometer || HUD == nullptr || HUD->Canvas == nullptr) { return; }
        APawn* P = HUD->GetOwningPawn();
        UCharacterMovementComponent* CMC = P ? P->FindComponentByClass<UCharacterMovementComponent>() : nullptr;
        if (!CMC) { return; }

        const float HSpeed = FVector(CMC->Velocity.X, CMC->Velocity.Y, 0.0f).Size();
        UCanvas* C = HUD->Canvas;
        const float X = C->SizeX * 0.5f - 60.0f;
        float Y = C->SizeY * 0.75f;

        C->SetDrawColor(255, 255, 255, 255);
        C->DrawText(GEngine->GetMediumFont(),
            FString::Printf(TEXT("%.0f %s"), HLConvertUnit(HSpeed, S.SpeedUnit), HLUnitSuffix(S.SpeedUnit)), X, Y);

        if (S.bSpeedometerJumpSpeeds)
        {
            const TArray<FHLJumpRecord>& Hist = FHLSpeedometerData::Get().History();
            for (int32 i = Hist.Num() - 1; i >= 0; --i)
            {
                Y += 18.0f;
                const FHLJumpRecord& R = Hist[i];
                if (R.Delta > 1.0f)      C->SetDrawColor(80, 255, 80, 255);   // gained -> green
                else if (R.Delta < -1.0f) C->SetDrawColor(255, 80, 80, 255);  // lost -> red
                else                      C->SetDrawColor(220, 220, 220, 255); // ~flat
                C->DrawText(GEngine->GetMediumFont(),
                    FString::Printf(TEXT("%.0f (%+.0f)"), HLConvertUnit(R.Speed, S.SpeedUnit), HLConvertUnit(R.Delta, S.SpeedUnit)), X, Y);
            }
        }
    }
}
```

- [ ] **Step 3: Hook the HUD draw** in `HLMovementHooks.cpp` — Satisfactory's HUD is `AFGHUD`; hook its draw. Add to `RegisterHooks()`:

```cpp
// Draw speedometer each frame after the game HUD draws.
SUBSCRIBE_METHOD_VIRTUAL_AFTER(AHUD::PostRender, GetMutableDefault<AHUD>(),
    [](auto& Scope, AHUD* HUD) { HLSpeedometer::DrawTo(HUD); });
```

> If `AHUD::PostRender` resolves to the wrong instance, fall back to hooking `AFGHUD::DrawHUD` (include `FGHUD.h`, sample `GetMutableDefault<AFGHUD>()`). Both are virtual; pick whichever compiles and draws.

- [ ] **Step 4: Build → Succeeded.**
- [ ] **Step 5: Manual verify (PIE):** live speed number visible; jumping appends color-coded entries (green when airstrafe gains, red when a hop loses speed).
- [ ] **Step 6: Commit** — `git commit -am "feat(hlmove): speedometer HUD with color-coded per-jump deltas"`

---

## Phase 6 — Parachute / jetpack momentum carry

### Task 15: Preserve horizontal momentum entering parachute/jetpack

**Files:** Modify `.../Private/HLMovementHooks.cpp`

- [ ] **Step 1: Extend the `OnMovementModeChanged` hook** (Task 13) — when entering parachute custom mode, stash & restore horizontal velocity so the transition can't zero it:

```cpp
// inside the OnMovementModeChanged AFTER handler, append:
if (Self->MovementMode == MOVE_Custom &&
    Self->CustomMovementMode == static_cast<uint8>(ECustomMovementMode::CMM_Parachute) &&
    S.bCarryParachute)
{
    // Vanilla may damp horizontal on entry; nothing to do if it doesn't. Confirm in PIE,
    // and if it resets, restore here using a cached pre-transition velocity (see Step 2).
}
```

- [ ] **Step 2: Cache pre-transition velocity** — add a BEFORE hook on `OnMovementModeChanged` to capture the outgoing velocity, then restore in the AFTER hook if it was zeroed:

```cpp
static FVector GHLPreTransitionVel = FVector::ZeroVector;

SUBSCRIBE_METHOD_VIRTUAL(UFGCharacterMovementComponent::OnMovementModeChanged, Sample,
    [](auto& Scope, UFGCharacterMovementComponent* Self, EMovementMode PrevMode, uint8 PrevCustom)
    {
        GHLPreTransitionVel = Self ? Self->Velocity : FVector::ZeroVector;
    });

// in the AFTER handler, for parachute/jetpack, if horizontal got reset, restore it:
const bool bIsParachute = Self->MovementMode == MOVE_Custom &&
    Self->CustomMovementMode == static_cast<uint8>(ECustomMovementMode::CMM_Parachute);
if ((bIsParachute && S.bCarryParachute))
{
    const float NewH = FVector(Self->Velocity.X, Self->Velocity.Y, 0).Size();
    const float OldH = FVector(GHLPreTransitionVel.X, GHLPreTransitionVel.Y, 0).Size();
    if (NewH < OldH * 0.5f) { Self->Velocity.X = GHLPreTransitionVel.X; Self->Velocity.Y = GHLPreTransitionVel.Y; }
}
```

> Jetpack thrust is equipment-driven and modifies velocity additively rather than via a custom move-mode transition, so it preserves horizontal momentum already; verify in PIE and only add handling if it doesn't. `bCarryJetpack` is reserved for that case.

- [ ] **Step 3: Parachute drag tuning** — reduce horizontal bleed while parachuting by re-using the air branch. In the Task 10 `CalcVelocity` hook, add a parachute case BEFORE the `MOVE_Walking/Falling` early-return so carried speed glides:

```cpp
if (Mode == MOVE_Custom &&
    Self->CustomMovementMode == static_cast<uint8>(ECustomMovementMode::CMM_Parachute) &&
    S.bCarryParachute)
{
    // gentle horizontal drag instead of vanilla's stronger scrub
    FVector V = Self->Velocity;
    const float DragPerSec = S.Params.Friction * S.ParachuteDragScale;
    V.X -= V.X * DragPerSec * Dt;
    V.Y -= V.Y * DragPerSec * Dt;
    Self->Velocity = V;
    return;   // let vanilla parachute handle vertical; do NOT Cancel (vanilla still needs to run vertical)
}
```

> Important: here we **do not** `Scope.Cancel()` — vanilla parachute physics still must run for descent. We only nudge horizontal before it. Verify the glide feel in PIE and tune `ParachuteDragScale`.

- [ ] **Step 4: Build → Succeeded.**
- [ ] **Step 5: Manual verify (PIE):** build speed on ground, deploy parachute mid-flight → horizontal speed carries and bleeds slowly (glide far). Jetpack from speed → momentum retained.
- [ ] **Step 6: Commit** — `git commit -am "feat(hlmove): carry momentum into parachute/jetpack with glide drag"`

---

## Phase 7 — Surf verification & optional slide hook

### Task 16: Verify surf; add `SlideAlongSurface` hook only if needed

**Files:** Modify `.../Private/HLMovementHooks.cpp` (conditional)

- [ ] **Step 1: Manual verify (PIE)** on a steep ramp (build one with a foundation tilted past the walkable angle, or use existing terrain): jump into it while airborne — you should slide along it and retain/redirect speed with airstrafe.
- [ ] **Step 2:** If vanilla scrubs speed on the ramp, add a slide hook that uses pure clip:

```cpp
SUBSCRIBE_METHOD_VIRTUAL(UFGCharacterMovementComponent::SlideAlongSurface, Sample,
    [](auto& Scope, UFGCharacterMovementComponent* Self, const FVector& Delta, float Time, const FVector& Normal, FHitResult& Hit, bool bHandleImpact)
    {
        FHLMovementSettings& S = FHLMovementSettings::Get();
        if (!S.bEnabled || Self == nullptr || Self->MovementMode != MOVE_Falling) { return; }
        // Pure slide: clip velocity to plane, keep magnitude (overbounce 1.0).
        Self->Velocity = HLMovementCore::ClipVelocity(Self->Velocity, Normal, 1.0f);
        // Let vanilla still perform the geometric slide move:
        // (do not Cancel; we only adjusted velocity for next tick)
    });
```

- [ ] **Step 3: Build → Succeeded. Step 4: Re-verify surf in PIE.**
- [ ] **Step 5: Commit** — `git commit -am "feat(hlmove): surf — pure clip-to-plane on steep slides"`

---

## Phase 8 — SML config menu (editor) + wire to settings

### Task 17: Author the SML config asset and sync to `FHLMovementSettings`

SML config is asset-based: a `UModConfiguration` (and a generated struct) authored in the editor, read via `UConfigManager`. (See `Mods/SML/Source/SML/Public/Configuration/ModConfiguration.h` and `ConfigManager.h`.)

**Files:** editor-created config asset under `Mods/HalfLifeMovement/Config/`; Modify `HLMovementSettings.cpp` to pull values.

- [ ] **Step 1 (editor):** Create a Mod Configuration asset for `HalfLifeMovement` with properties mirroring §7 of the spec (HL-named labels: `sv_accelerate`, `sv_airaccelerate`, `sv_friction`, `sv_stopspeed`, `airspeedcap`, `sv_maxspeed`, `sprint_maxspeed`, `Enabled`, `autohop`, `carry_parachute`, `carry_jetpack`, `parachute_drag_scale`, `speedometer_enabled`, `speedometer_show_jump_speeds`, `speedometer_unit`).
- [ ] **Step 2 (code):** Add a function `FHLMovementSettings::SyncFromConfig(const FConfigStruct&)` that copies asset values into the live struct, and register a config-changed handler so edits in the menu update `FHLMovementSettings::Get()` live. (Use the generated struct type from Step 1; wire via `UConfigManager::GetConfigurationStruct`.)
- [ ] **Step 3: Build → Succeeded.**
- [ ] **Step 4: Manual verify (PIE):** open Mods → Half-Life Movement config; changing `sv_airaccelerate` / `sv_maxspeed` / `airspeedcap` changes the feel live; toggling `Enabled` reverts to vanilla; speedometer toggles respond.
- [ ] **Step 5: Commit** — `git add -A && git commit -m "feat(hlmove): SML config menu wired to live settings"`

---

## Phase 9 — Package & play in the campaign

### Task 18: Alpakit build + install, verify in the real game

- [ ] **Step 1 (editor):** Open the Alpakit panel, select **HalfLifeMovement**, Alpakit it (Development), with "copy to game" enabled so it lands in the game's `Mods/` folder.
- [ ] **Step 2:** Launch Satisfactory (Steam). Confirm the mod loads (Mod Manager / in-game mod list shows Half-Life Movement).
- [ ] **Step 3: Manual verify in a real save:** bhop, airstrafe, ABH, surf, speedometer, parachute/jetpack carry all work; tune constants via the config menu to taste.
- [ ] **Step 4: Commit any final tuning defaults** — `git commit -am "chore(hlmove): tuned default constants for campaign feel"`

---

## Self-review notes (coverage vs spec)

- §2 scope items map to tasks: ground accel/friction (T2–T3, T10), air-strafe/ABH/forward-preserve (T4, T10), auto-bhop + friction ordering (T13), surf/slopes (T5, T16), sprint reuse (T10 via `GetIsSprinting`), parachute/jetpack carry (T15), config menu (T17), speedometer + color (T12, T14), master toggle (T8/T11/T17), units (T14).
- §3 constraint honored: no subclassing; `SUBSCRIBE_METHOD_VIRTUAL` hooks only.
- Risk §8.1 (friction ordering) has an explicit fallback in T13. Risk §8.3 (surf) is T16's conditional hook. Risk §8.2 (prediction) is covered by PIE verification at each phase; multiplayer is out of scope.
- Known soft spots requiring on-hardware confirmation (called out inline, not hidden): exact `OnMovementModeChanged` ordering for friction pre-empt (T13), whether vanilla resets velocity into parachute (T15), HUD hook target `AHUD::PostRender` vs `AFGHUD::DrawHUD` (T14), and the SML config generated-struct accessor name (T17). Each has a stated fallback.
```
