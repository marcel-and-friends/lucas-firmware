#pragma once

#include <string_view>
#include <src/MarlinCore.h>
#include <span>

namespace lucas::info {
void tick(millis_t tick);

void interpretar_json(std::span<char> buffer);
}
