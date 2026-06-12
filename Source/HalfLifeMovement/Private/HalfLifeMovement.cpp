#include "HalfLifeMovement.h"
#include "HLMovementHooks.h"

#define LOCTEXT_NAMESPACE "FHalfLifeMovementModule"

void FHalfLifeMovementModule::StartupModule()
{
	HLMovementHooks::RegisterHooks();
}

void FHalfLifeMovementModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FHalfLifeMovementModule, HalfLifeMovement)
