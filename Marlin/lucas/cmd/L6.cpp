#include <lucas/cmd/cmd.h>
#include <lucas/storage/storage.h>
#include <src/gcode/parser.h>

namespace lucas::cmd {
void L6() {
    if (not parser.seenval('P') ||
        not parser.seenval('M'))
        return;

    const auto pin = parser.intval('P');
    const auto mode = parser.intval('M');
    pinMode(pin, mode);

    if (parser.seen('W')) {
        const auto value = parser.longval('V', -1);
        if (value != -1)
            analogWrite(pin, value);

        LOG("pino modificado - [pino = ", pin, " | modo = ", mode, " | valor = ", value, "]");
    } else if (parser.seen('R')) {
        const auto p = digitalPinToPinName(pin);
        if (pin_in_pinmap(p, PinMap_ADC) and mode == 4) {
            LOG("pino #", pin, " (ADC) valor = ", analogRead(pin));
        } else {
            LOG("pino #", pin, " valor = ", digitalRead(pin));
        }
    }
}
}
