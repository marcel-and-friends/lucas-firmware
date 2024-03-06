#include <lucas/cmd/cmd.h>
#include <lucas/RecipeQueue.h>
#include <lucas/Spout.h>
#include <lucas/Recipe.h>
#include <lucas/cfg/cfg.h>
#include <lucas/sec/sec.h>
#include <src/gcode/parser.h>

namespace lucas::cmd {
void L4() {
    bool updated = false;
    bool updated_maintenance_mode = false;
    for (auto& option : cfg::options()) {
        if (option.id == cfg::Option::ID_DEFAULT)
            continue;

        if (parser.seen(option.id)) {
            bool value = parser.value_bool();
            if (option.active != value) {
                option.active = value;
                LOG("option \'", AS_CHAR(option.id), "\' was ", option.active ? "" : "un", "set");
                updated = true;

                if (option.id == cfg::options()[usize(cfg::Options::MaintenanceMode)].id)
                    updated_maintenance_mode = true;
            }
        }
    }

    if (updated)
        cfg::save_options();

    if (updated_maintenance_mode) {
        SERIAL_IMPL.flush();
        noInterrupts();
        NVIC_SystemReset();
    }
}
}
