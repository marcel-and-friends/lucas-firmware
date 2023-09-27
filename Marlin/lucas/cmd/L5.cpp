#include <lucas/cmd/cmd.h>
#include <src/gcode/parser.h>
#include <lucas/Spout.h>
#include <lucas/Boiler.h>
#include <lucas/TickFilter.h>
#include <lucas/Station.h>

namespace lucas::cmd {
void L5() {
    auto test_type = parser.intval('T');
    auto status = parser.intval('S');
    switch (test_type) {
    case 0: {
        if (status) {
            Spout::the().pour_with_digital_signal(10min, 1400);
        } else {
            Spout::the().end_pour();
        }
    } break;
    case 1: {
        auto filter = status ? TickFilter::Boiler : TickFilter::None;
        apply_filters(filter);
        Boiler::the().control_resistance(status);
    } break;
    case 2: {
        static bool s_last_state = false;
        analogWrite(Station::list().at(status).led(), s_last_state * 4095);
        s_last_state = not s_last_state;
    } break;
    case 3: {
        static bool s_last_state = false;
        // POWERLED
        analogWrite(Station::list().at(status).led(), s_last_state * 4095);
        s_last_state = not s_last_state;
    } break;
    };
}
}
