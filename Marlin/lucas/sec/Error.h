#pragma once

#include <lucas/types.h>

namespace lucas::sec {
enum class Error : u8 {
    MaxTemperatureReached = 0,
    TemperatureNotChanging,
    TemperatureOutOfRange,
    WaterLevelAlarm,
    PourVolumeMismatch,
    Count,
    Invalid
};
}
