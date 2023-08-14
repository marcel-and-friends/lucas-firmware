#include "serial.h"
#include <lucas/lucas.h>
#include <lucas/core/core.h>
#include <lucas/info/info.h>
#include <lucas/serial/HookDelimitado.h>
#include <lucas/serial/UniquePriorityHook.h>
#include <src/core/serial.h>

namespace lucas::serial {
void setup() {
    HookDelimitado::make(
        '#',
        [](std::span<char> buffer) {
            info::interpretar_json(buffer);
        });

    limpar_serial();
}

bool hooks() {
    constexpr auto MAX_BYTES = 64;
    static HookDelimitado* s_hook_ativo = nullptr;

    if (UniquePriorityHook::the()) {
        while (SERIAL_IMPL.available()) {
            auto& hook = *UniquePriorityHook::the();
            hook.receive_char(SERIAL_IMPL.read());
        }

        return true;
    }

    while (SERIAL_IMPL.available()) {
        auto const peek = SERIAL_IMPL.peek();
        if (not s_hook_ativo) {
            LOG_IF(LogSerial, "procurando hook - peek = ", AS_CHAR(peek));

            HookDelimitado::for_each([&](auto& hook) {
                if (hook.delimitador() == peek) {
                    SERIAL_IMPL.read();

                    s_hook_ativo = &hook;
                    s_hook_ativo->begin();

                    LOG_IF(LogSerial, "iniciando leitura");

                    return util::Iter::Break;
                }
                return util::Iter::Continue;
            });
        } else {
            auto& hook = *s_hook_ativo;
            if (hook.delimitador() == peek) {
                SERIAL_IMPL.read();

                hook.dispatch();

                s_hook_ativo = nullptr;
                LOG_IF(LogSerial, "finalizando leitura");
            } else {
                hook.add_to_buffer(SERIAL_IMPL.read());
            }
        }

        if (not s_hook_ativo)
            break;
    }
    return s_hook_ativo;
}

void limpar_serial() {
    while (SERIAL_IMPL.available())
        SERIAL_IMPL.read();
}
}
