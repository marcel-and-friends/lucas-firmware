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
    // { // boiler
    //     if (not filtered(TickFilter::Boiler))
    //         Boiler::the().tick();
    // }

    // { // queue
    //     if (not filtered(TickFilter::RecipeQueue))
    //         RecipeQueue::the().tick();

    //     RecipeQueue::the().remove_finalized_recipes();
    // }

    // { // stations
    //     if (not filtered(TickFilter::Station))
    //         Station::tick();

    //     Station::update_leds();
    // }

    // { // spout
    //     if (not filtered(TickFilter::Spout))
    //         Spout::the().tick();
    // }

    // { // info
    //     if (not filtered(TickFilter::Info))
    //         info::tick();
    // }
}
}
