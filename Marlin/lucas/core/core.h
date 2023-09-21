#pragma once

#include <lucas/types.h>

namespace lucas::core {
enum class CalibrationPhase {
    WaitingForTemperatureToStabilize,
    FillingDigitalSignalTable,

    None
};

void setup();

void tick();

void calibrate(float target_temperature);

void inform_calibration_status();

void prepare_for_firmware_update(usize size);
}
