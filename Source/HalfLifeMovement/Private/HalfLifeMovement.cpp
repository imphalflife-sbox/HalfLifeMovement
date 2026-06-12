#include "HalfLifeMovement.h"
#include "HLMovementHooks.h"
#include "HLMovementConsole.h"

#define LOCTEXT_NAMESPACE "FHalfLifeMovementModule"

void FHalfLifeMovementModule::StartupModule()
{
	HLMovementConsole::LoadSettings();   // restore persisted settings (before cvars bind to the fields)
	HLMovementConsole::RegisterCVars();  // editor-safe; enables live HL.* tuning in-game
	HLMovementHooks::RegisterHooks();    // native hooks; only install outside the editor
}

void FHalfLifeMovementModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FHalfLifeMovementModule, HalfLifeMovement)
