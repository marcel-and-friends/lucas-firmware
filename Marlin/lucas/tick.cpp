#include "tick.h"
#include <lucas/lucas.h>
#include <lucas/core/core.h>
#include <lucas/Station.h>
#include <lucas/RecipeQueue.h>
#include <lucas/Spout.h>
#include <lucas/Boiler.h>
#include <lucas/info/info.h>

namespace lucas {
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
