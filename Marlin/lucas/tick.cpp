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
        if (not filtered(Filters::Boiler))
            Boiler::the().tick();
    }

    { // queue
        if (not filtered(Filters::RecipeQueue))
            RecipeQueue::the().tick();

        RecipeQueue::the().remove_finalized_recipes();
    }

    { // stations
        if (not filtered(Filters::Station))
            Station::tick();

        Station::update_leds();
    }

    { // spout
        if (not filtered(Filters::Spout))
            Spout::the().tick();
    }

    { // info
        if (not filtered(Filters::Info))
            info::tick();
    }
}
}
