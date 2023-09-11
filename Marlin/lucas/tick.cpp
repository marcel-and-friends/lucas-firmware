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
    constexpr auto PINS = std::array{
        X_ENABLE_PIN,
        Y_ENABLE_PIN,
        Z_ENABLE_PIN,
        E0_ENABLE_PIN,
        E1_ENABLE_PIN,
        PC5,
        PC6,
        PD11,
        PA5,
        PE6
    };

    static bool last_state = false;
    static bool once = false;
    every(2s) {
        if (not once) {
            for (auto pin : PINS)
                pinMode(pin, OUTPUT);
            once = true;
        }

        for (auto pin : PINS)
            analogWrite(pin, last_state * 4095);

        last_state = not last_state;
        LOG("oi 3");
    }

    { // boiler
        if (not filtered(TickFilter::Boiler))
            Boiler::the().tick();
    }

    { // queue
        if (not filtered(TickFilter::RecipeQueue))
            RecipeQueue::the().tick();

        RecipeQueue::the().remove_finalized_recipes();
    }

    { // stations
        if (not filtered(TickFilter::Station))
            Station::tick();

        Station::update_leds();
    }

    { // spout
        if (not filtered(TickFilter::Spout))
            Spout::the().tick();
    }

    { // info
        if (not filtered(TickFilter::Info))
            info::tick();
    }
}
}
