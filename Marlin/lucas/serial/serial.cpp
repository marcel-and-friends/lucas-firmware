#include "serial.h"
#include <lucas/lucas.h>
#include <lucas/core/core.h>
#include <lucas/info/info.h>
#include <lucas/serial/DelimitedHook.h>
#include <lucas/serial/UniquePriorityHook.h>
#include <src/core/serial.h>

namespace lucas::serial {
void setup() {
    DelimitedHook::make(
        '#',
        [](std::span<char> buffer) {
            info::interpret_json(buffer);
        });

    limpar_serial();
}

bool hooks() {
    static DelimitedHook* s_active_hook = nullptr;

    if (UniquePriorityHook::the()) {
        while (SERIAL_IMPL.available())
            UniquePriorityHook::the()->receive_char(SERIAL_IMPL.read());

        return true;
    }

    while (SERIAL_IMPL.available()) {
        auto const peek = SERIAL_IMPL.peek();
        if (not s_active_hook) {
            LOG_IF(LogSerial, "procurando hook - peek = ", AS_CHAR(peek));

            DelimitedHook::for_each([&](auto& hook) {
                if (hook.delimiter() == peek) {
                    SERIAL_IMPL.read();

                    s_active_hook = &hook;
                    s_active_hook->begin();

                    LOG_IF(LogSerial, "iniciando leitura");

                    return util::Iter::Break;
                }
                return util::Iter::Continue;
            });
        } else {
            auto& hook = *s_active_hook;
            if (hook.delimiter() == peek) {
                SERIAL_IMPL.read();

                hook.dispatch();

                s_active_hook = nullptr;
                LOG_IF(LogSerial, "finalizando leitura");
            } else {
                hook.add_to_buffer(SERIAL_IMPL.read());
            }
        }

        if (not s_active_hook)
            break;
    }
    return s_active_hook;
}

void limpar_serial() {
    while (SERIAL_IMPL.available())
        SERIAL_IMPL.read();
}
}
