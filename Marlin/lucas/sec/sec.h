#pragma once

#include <lucas/sec/Reason.h>

namespace lucas::sec {
void setup();

void raise_error(Reason);

bool is_reason_blocked(Reason);

void toggle_reason_block(Reason);
}
