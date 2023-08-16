#include <lucas/cmd/cmd.h>
#include <lucas/RecipeQueue.h>
#include <lucas/Spout.h>
#include <lucas/Recipe.h>
#include <lucas/cfg/cfg.h>

namespace lucas::cmd {
void L4() {
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

    if (parser.seen('Z')) {
        switch (parser.value_int()) {
        case 0: {
            Spout::FlowController::the().clean_digital_signal_table(Spout::FlowController::SaveToFlash::Yes);
            LOG("tabela foi limpa e salva na flash");
        } break;
        case 1: {
            cfg::reset_options();
            LOG("opcoes foram resetadas e salvas na flash");
        } break;
        }
    }
}
}
