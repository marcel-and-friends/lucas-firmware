#include <lucas/cmd/cmd.h>
#include <lucas/RecipeQueue.h>
#include <lucas/Spout.h>
#include <lucas/Recipe.h>
#include <lucas/cfg/cfg.h>

namespace lucas::cmd {
void L4() {
    if (parser.seenval('Z')) {
        switch (parser.value_int()) {
        case 0: {
            Spout::FlowController::the().clean_digital_signal_table(Spout::FlowController::SaveToFlash::Yes);
            LOG("tabela foi limpa e salva na flash");
        } break;
        case 1: {
            cfg::reset_options();
            LOG("opcoes foram resetadas e salvas na flash");
        } break;
        case 2: {
            if (not parser.seenval('P'))
                return;

            const auto pin = parser.value_int();
            const auto mode = parser.seenval('T') ? parser.value_int() : OUTPUT;
            const auto value = parser.seenval('V') ? parser.value_long() : -1;

            pinMode(pin, mode);
            if (value != -1)
                analogWrite(pin, value);

            LOG("aplicando modificacoes - [pino = ", pin, " | modo = ", mode, " | valor = ", value, "]");
        } break;
        case 3: {
            if (not parser.seenval('P'))
                return;

            const auto pin = parser.value_int();
            LOG("pino #", pin, " valor = ", READ(pin));
        } break;
        }
        return;
    }

    bool updated = false;
    for (auto& option : cfg::opcoes()) {
        if (option.id != cfg::Option::ID_DEFAULT and parser.seen(option.id)) {
            option.active = not option.active;
            LOG("option \'", AS_CHAR(option.id), "\' foi ", not option.active ? "des" : "", "ativada");
            updated = true;
        }
    }

    if (updated)
        cfg::save_options_to_flash();
}
}
