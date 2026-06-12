#pragma once

namespace HLMovementConsole
{
    // Registers HL.* console variables/commands bound to the live mod settings,
    // for in-game tuning. Editor-safe (no native hooks), call from StartupModule.
    void RegisterCVars();
}
