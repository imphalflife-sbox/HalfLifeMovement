#include "HalfLifeMovement.h"

#define LOCTEXT_NAMESPACE "FHalfLifeMovementModule"

void FHalfLifeMovementModule::StartupModule()
{
	// Hook registration is added in a later task (Phase 2).
}

void FHalfLifeMovementModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FHalfLifeMovementModule, HalfLifeMovement)
