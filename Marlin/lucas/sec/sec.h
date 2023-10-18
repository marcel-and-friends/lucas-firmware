#pragma once

#include <lucas/sec/Error.h>

namespace lucas::sec {
void setup();

void raise_error(Error);

bool has_active_error();

void inform_active_error();

bool is_reason_blocked(Error);

void toggle_reason_block(Error);
}
