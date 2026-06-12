#include "HLMovementSettings.h"

FHLMovementSettings& FHLMovementSettings::Get()
{
    static FHLMovementSettings Instance;
    return Instance;
}
