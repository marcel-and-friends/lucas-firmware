#include <lucas/cmd/cmd.h>
#include <lucas/RecipeQueue.h>
#include <lucas/Spout.h>
#include <lucas/Recipe.h>
#include <lucas/cfg/cfg.h>
#include <lucas/sec/sec.h>
#include <src/gcode/parser.h>

namespace lucas::cmd {
void L4() {
    if (parser.seenval('Z')) {
        switch (parser.value_int()) {
        case 0: {
            Spout::FlowController::the().clean_digital_signal_table(Spout::FlowController::RemoveFile::Yes);
            LOG("tabela foi limpa e salva");
        } break;
        case 1: {
            cfg::reset_options();
            LOG("opcoes foram resetadas e salvas");
        } break;
        case 2: {
            if (not parser.seenval('R'))
                return;

            const auto reason_number = parser.value_int();
            if (reason_number < 0 or reason_number >= usize(sec::Error::Count)) {
                LOG_ERR("reason invalida - max = ", usize(sec::Error::Count));
                return;
            }

            const auto reason = sec::Error(reason_number);
            sec::toggle_reason_block(reason);
            LOG("razao #", reason_number, " foi ", sec::is_reason_blocked(reason) ? "bloqueada" : "desbloqueada");
        } break;
        case 3: {
            if (not parser.seenval('P'))
                return;

            const auto pin = parser.value_int();
            const auto mode = parser.intval('M', OUTPUT);
            const auto value = parser.longval('V', -1);

            pinMode(pin, mode);
            if (value != -1)
                analogWrite(pin, value);

            LOG("pino modificado - [pino = ", pin, " | modo = ", mode, " | valor = ", value, "]");
        } break;
        case 4: {
            if (not parser.seenval('P'))
                return;

            const auto pin = parser.value_int();
            const auto p = digitalPinToPinName(pin);
            if (pin_in_pinmap(p, PinMap_ADC)) {
                LOG("pino #", pin, " (ADC) valor = ", analogRead(pin));
            } else {
                LOG("pino #", pin, " valor = ", digitalRead(pin));
            }
        } break;
        }
        return;
    }

    bool updated = false;
    for (auto& option : cfg::options()) {
        if (option.id != cfg::Option::ID_DEFAULT and parser.seen(option.id)) {
            option.active = not option.active;
            LOG("option \'", AS_CHAR(option.id), "\' foi ", not option.active ? "des" : "", "ativada");
            updated = true;
        }
    }

    if (updated)
        cfg::save_options();
}
}
