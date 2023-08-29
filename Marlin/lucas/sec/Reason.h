#pragma once

#include <cstdint>

namespace lucas::sec {
enum class Reason : uint32_t {
    MaxTemperatureReached,
    TemperatureNotChanging = 0,
    TemperatureOutOfRange,
    PourVolumeMismatch,
    WaterLevelAlarm,
    NoSDCard,
    Count
};
}
