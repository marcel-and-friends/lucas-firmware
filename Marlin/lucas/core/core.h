#pragma once

#include <span>

namespace lucas::core {
void setup();

void nivelar(float temperatura_target);

bool nivelado();

void solicitar_nivelamento();

void prepare_for_firmware_update(size_t size);
}
