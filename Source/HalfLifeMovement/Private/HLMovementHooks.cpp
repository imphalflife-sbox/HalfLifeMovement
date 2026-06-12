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
