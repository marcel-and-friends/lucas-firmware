#pragma once

#include <lucas/tick.h>
#include <lucas/util/util.h>
#include <lucas/cfg/cfg.h>

namespace lucas {
void setup();

void calibrate();

bool initialized();

bool initializing();
}
