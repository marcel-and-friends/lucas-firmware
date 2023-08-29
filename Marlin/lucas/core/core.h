#pragma once

#include <cstddef>

namespace lucas::core {
void setup();

void tick();

void calibrate(float target_temperature);

bool calibrated();

void inform_calibration_status();

void prepare_for_firmware_update(size_t size);
}
