#include "lucas.h"
#include <lucas/Spout.h>
#include <lucas/Boiler.h>
#include <lucas/RecipeQueue.h>
#include <lucas/serial/serial.h>
#include <lucas/sec/sec.h>
#include <lucas/info/info.h>
#include <lucas/core/core.h>

namespace lucas {
static bool s_initialized = false;
static bool s_initializing = false;

void setup() {
    core::TemporaryFilter f{ TickFilter::Interaction };
    s_initializing = true;

    cfg::setup();
    serial::setup();
    sec::setup();
    info::setup();
    core::setup();

    s_initialized = true;
}

bool initialized() {
    return s_initialized;
}

bool initializing() {
    return s_initializing;
}
}
