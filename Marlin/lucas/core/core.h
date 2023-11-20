#pragma once

#include <lucas/types.h>
#include <optional>

namespace lucas::core {
void setup();

void tick();

void calibrate(std::optional<s32> target_temperature);

enum class CalibrationPhase {
    None,
    ReachingTargetTemperature,
    AnalysingFlowData,
    Done
};

CalibrationPhase calibration_phase();

void inform_calibration_status();

void prepare_for_firmware_update(usize size);
}
