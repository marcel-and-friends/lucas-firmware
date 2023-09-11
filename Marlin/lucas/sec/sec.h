#pragma once

#include <lucas/sec/Reason.h>

namespace lucas::sec {
void setup();

void raise_error(Reason);

Reason active_error();

void inform_active_error(bool state);

bool is_reason_blocked(Reason);

void toggle_reason_block(Reason);
}
