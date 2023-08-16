#pragma once

#include <cstddef>

namespace lucas::core {
void setup();

void calibrate(float target_temperature);

bool calibrated();

void request_calibration();

void prepare_for_firmware_update(size_t size);
}
