#include <lucas/cmd/cmd.h>
#include <src/gcode/parser.h>
#include <lucas/Spout.h>
#include <lucas/Boiler.h>
#include <lucas/TickFilter.h>
#include <lucas/Station.h>
#include <src/libs/buzzer.h>

namespace lucas::cmd {
void L5() {
    auto test_type = parser.intval('T');
    auto status = parser.intval('S');
    auto value = parser.intval('V');
    switch (test_type) {
    case 0: {
        if (status) {
            Spout::the().pour_with_digital_signal(10min, value);
        } else {
            Spout::the().end_pour();
        }
        LOG("DESPEJO: ", status ? "iniciou" : "terminou");
    } break;
    case 1: {
        auto filter = status ? TickFilter::Boiler : TickFilter::None;
        apply_filters(filter);
        Boiler::the().control_resistance(status);
        LOG("RESISTENCIA: ", status ? "ligou" : "desligou");
    } break;
    case 2: {
        static bool s_last_state[Station::MAXIMUM_NUMBER_OF_STATIONS] = {};

        auto& state = s_last_state[value];
        analogWrite(Station::list().at(value).led(), state * 4095);

        LOG("LED #", value + 1, ": ", state ? "ligou" : "desligou");

        state = not state;
    } break;
    case 3: {
        static bool s_last_state[Station::MAXIMUM_NUMBER_OF_STATIONS] = {};

        auto& state = s_last_state[value];
        analogWrite(Station::list().at(value).powerled(), state * 4095);

        LOG("POWERLED #", value + 1, ": ", state ? "ligou" : "desligou");

        state = not state;
    } break;
    case 4: {
        CFG(LogFlowSensorDataForTesting) = status;
        LOG("LOG SENSOR DE FLUXO: ", status ? "ativado" : "desativado");
    } break;
    case 5: {
        CFG(LogTemperatureForTesting) = status;
        LOG("LOG SENSOR DE TEMPERATURA: ", status ? "ativado" : "desativado");
    } break;
    case 6: {
        status ? tone(BEEPER_PIN, value, 10000) : noTone(BEEPER_PIN, true);
        LOG("BEEPER: ", status ? "ativado" : "desativado");
    } break;
    };
}
}
