#include "lucas.h"
#include <lucas/core/core.h>
#include <lucas/Spout.h>
#include <lucas/Boiler.h>
#include <lucas/RecipeQueue.h>
#include <lucas/serial/serial.h>
#include <lucas/info/info.h>
#include <lucas/wifi/wifi.h>

namespace lucas {
static bool s_initialized = false;

void setup() {
    util::TemporaryFilter f{ Filters::Interaction };

    cfg::setup();
    serial::setup();
    info::setup();
    core::setup();

    s_initialized = true;
}

bool initialized() {
    return s_initialized;
}
}
