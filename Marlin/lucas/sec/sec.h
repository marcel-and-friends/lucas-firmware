#pragma once

#include <lucas/sec/Error.h>

namespace lucas::sec {
void setup();

void tick();

void raise_error(Error);
}
