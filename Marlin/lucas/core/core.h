#pragma once

#include <span>

namespace lucas::core {
void setup();

void nivelar(float temperatura_target);

bool nivelado();

void solicitar_nivelamento();

void add_buffer_to_new_firmware_file(std::span<char>);
}
