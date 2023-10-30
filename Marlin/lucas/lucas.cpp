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
    core::TemporaryFilter f{ core::Filter::RecipeQueue, core::Filter::Station };
    s_setup_state = SetupState::Started;

    cfg::setup();
    serial::setup();
    sec::setup();
    info::setup();
    core::setup();

    s_setup_state = SetupState::Done;
}

void tick() {
    sec::tick();
    core::tick();
    if (not is_filtered(core::Filter::Info))
        info::tick();
}

SetupState setup_state() {
    return s_setup_state;
}
}
