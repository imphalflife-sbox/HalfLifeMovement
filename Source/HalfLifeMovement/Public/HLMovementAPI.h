#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"

class ACharacter;

/**
 * Public API for other mods (since 1.1.0). HL drives the LOCAL player's movement only, so both calls
 * act on the local player and do nothing for anyone else.
 */
namespace HLMovementAPI
{
	/**
	 * The latest steep surface the character surfed or slid along: a hit UE slid the falling player
	 * along this frame, or the surf floor trace below them. False if there was none in the last 0.1 s.
	 * Near-vertical faces count too (the floor trace alone misses them).
	 */
	HALFLIFEMOVEMENT_API bool GetSurfContact(const ACharacter* Character, FHitResult& OutHit);

	/**
	 * Adds DeltaV to the character's velocity. While surfing, HL carries its own velocity and asserts it
	 * every frame, so a plain change to CharacterMovement->Velocity would be lost; this adds to both.
	 */
	HALFLIFEMOVEMENT_API void AddVelocity(ACharacter* Character, const FVector& DeltaV);
}
