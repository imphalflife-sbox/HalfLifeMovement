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
