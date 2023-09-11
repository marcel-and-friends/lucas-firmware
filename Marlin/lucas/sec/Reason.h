#pragma once

#include <lucas/types.h>

namespace lucas::sec {
enum class Reason : u8 {
    MaxTemperatureReached = 0,
    TemperatureNotChanging,
    TemperatureOutOfRange,
    WaterLevelAlarm,
    PourVolumeMismatch,
    NoSDCard,
    Count,
    Invalid
};
}
