#pragma once

#include <lucas/util/util.h>
#include <lucas/cfg/cfg.h>

namespace lucas {
enum class SetupState {
    NotStarted,
    Started,
    Done,
};

void setup();

void tick();

SetupState setup_state();
}
