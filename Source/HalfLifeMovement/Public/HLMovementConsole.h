#pragma once

namespace HLMovementConsole
{
    // Registers HL.* console variables/commands bound to the live mod settings,
    // for in-game tuning. Editor-safe (no native hooks), call from StartupModule.
    void RegisterCVars();

    // Load persisted settings from disk into FHLMovementSettings. Call BEFORE RegisterCVars so the
    // cvars bind to the loaded values. Save them with the HL.Save console command.
    void LoadSettings();
    void SaveSettings();
}
