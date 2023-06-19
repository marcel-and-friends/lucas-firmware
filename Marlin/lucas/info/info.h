#pragma once

#include <lucas/info/Report.h>
#include <span>

namespace lucas::info {
constexpr size_t BUFFER_SIZE = 1024;
using DocumentoJson = StaticJsonDocument<BUFFER_SIZE>;

void tick();

void interpretar_json(std::span<char> buffer);
}
