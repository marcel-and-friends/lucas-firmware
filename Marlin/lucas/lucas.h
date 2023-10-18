#pragma once

#include <lucas/util/util.h>
#include <lucas/cfg/cfg.h>

namespace lucas {
void setup();

void tick();

void calibrate();

bool initialized();

bool initializing();
}
