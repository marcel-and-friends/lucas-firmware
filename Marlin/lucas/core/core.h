#pragma once

#include <lucas/types.h>

namespace lucas::core {
void setup();

void tick();

void calibrate(float target_temperature);

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
