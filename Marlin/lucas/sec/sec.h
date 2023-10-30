#pragma once

#include <lucas/sec/Error.h>

namespace lucas::sec {
void setup();

void tick();

void raise_error(Error);

bool has_active_error();

void inform_active_error();

void remove_stored_error();
}
