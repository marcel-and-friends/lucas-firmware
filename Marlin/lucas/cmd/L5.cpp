#include <lucas/cmd/cmd.h>
#include <src/gcode/parser.h>
#include <lucas/Spout.h>
#include <lucas/Boiler.h>
#include <lucas/core/Filter.h>
#include <lucas/Station.h>
#include <src/libs/buzzer.h>

namespace lucas::cmd {
void L5() {
    auto test_type = parser.intval('T');
    auto status = parser.intval('S');
    auto value = parser.intval('V');
    switch (test_type) {
    case 0: {
        Spout::the().send_digital_signal_to_driver(status ? value : 0);
        LOG("DESPEJO(", Spout::Pin::SV, ", ", Spout::Pin::BRK, ", ", Spout::Pin::EN, "): ", status ? "iniciou" : "terminou");
    } break;
    case 1: {
        analogWrite(Boiler::Pin::Resistance, status * 255);
        LOG("RESISTENCIA(", Boiler::Pin::Resistance, "): ", status ? "ligou" : "desligou");
    } break;
    case 2: {
        static bool s_leds_state[Station::MAXIMUM_NUMBER_OF_STATIONS] = { HIGH, HIGH, HIGH, HIGH, HIGH };

        auto& state = s_leds_state[value];
        digitalWrite(Station::list().at(value).led(), state);

        LOG("LED #", value + 1, "(", Station::list().at(value).led(), "): ", state ? "ligou" : "desligou");

        state = not state;
    } break;
    case 3: {
        static bool s_powerleds_state[Station::MAXIMUM_NUMBER_OF_STATIONS] = { LOW, LOW, LOW, LOW, LOW };

        auto& state = s_powerleds_state[value];
        digitalWrite(Station::list().at(value).powerled(), state);

        LOG("POWERLED #", value + 1, ": ", not state ? "ligou" : "desligou");

        state = not state;
    } break;
    case 4: {
        CFG(LogFlowSensorDataForTesting) = status;
        cfg::save_options();
        LOG("LOG SENSOR DE FLUXO: ", status ? "ativado" : "desativado");
    } break;
    case 5: {
        CFG(LogTemperatureForTesting) = status;
        cfg::save_options();
        LOG("LOG SENSOR DE TEMPERATURA: ", status ? "ativado" : "desativado");
    } break;
    case 6: {
        status ? tone(BEEPER_PIN, value, 10000) : noTone(BEEPER_PIN, true);
        LOG("BEEPER(", BEEPER_PIN, "): ", status ? "ativado" : "desativado");
    } break;
    case 7: {
        status ? Boiler::the().update_target_temperature(value) : Boiler::the().turn_off_resistance();
        LOG("BOILER: maquina vai ", status ? "aquecer" : "esfriar");
    } break;
    default:
        break;
    };
}
}
