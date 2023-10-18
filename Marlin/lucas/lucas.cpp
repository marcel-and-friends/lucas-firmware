#include "lucas.h"
#include <lucas/Spout.h>
#include <lucas/Boiler.h>
#include <lucas/RecipeQueue.h>
#include <lucas/serial/serial.h>
#include <lucas/sec/sec.h>
#include <lucas/info/info.h>
#include <lucas/core/core.h>

namespace lucas {
static auto s_setup_state = SetupState::NotStarted;

void setup() {
    core::TemporaryFilter f{ core::Filter::Interaction };
    s_setup_state = SetupState::Started;

    cfg::setup();
    serial::setup();
    sec::setup();
    info::setup();
    core::setup();

    s_setup_state = SetupState::Done;
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
}
