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
    core::TemporaryFilter f{ core::Filter::Interaction };
    s_initializing = true;

    cfg::setup();
    serial::setup();
    sec::setup();
    info::setup();
    core::setup();

    s_initialized = true;
}

void tick() {
    { // boiler
        if (not is_filtered(core::Filter::Boiler))
            Boiler::the().tick();
    }

    { // queue
        if (not is_filtered(core::Filter::RecipeQueue))
            RecipeQueue::the().tick();

        RecipeQueue::the().remove_finalized_recipes();
    }

    { // stations
        if (not is_filtered(core::Filter::Station))
            Station::tick();

        Station::update_leds();
    }

    { // spout
        if (not is_filtered(core::Filter::Spout))
            Spout::the().tick();
    }

    { // info
        if (not is_filtered(core::Filter::Info))
            info::tick();
    }
}

bool initialized() {
    return s_initialized;
}

bool initializing() {
    return s_initializing;
}
}
