#include "serial.h"
#include <lucas/lucas.h>
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

    while (SERIAL_IMPL.available()) {
        auto const peek = SERIAL_IMPL.peek();
        if (not s_hook_ativo) {
            LOG_IF(LogSerial, "procurando hook - peek = ", AS_CHAR(peek));

            HookDelimitado::for_each([&](auto& hook) {
                if (hook.delimitador == peek) {
                    SERIAL_IMPL.read();

                    s_hook_ativo = &hook;
                    s_hook_ativo->counter = 1;

                    LOG_IF(LogSerial, "iniciando leitura");

                    return util::Iter::Break;
                }
                return util::Iter::Continue;
            });
        } else {
            auto& hook = *s_hook_ativo;
            if (hook.delimitador == peek) {
                SERIAL_IMPL.read();
                if (hook.callback and hook.buffer_size) {
                    hook.buffer[hook.buffer_size] = '\0';

                    LOG_IF(LogSerial, "!BUFFER!\n", hook.buffer);
                    LOG_IF(LogSerial, "!BUFFER!");

                    hook.callback({ hook.buffer, hook.buffer_size });
                }

                hook.reset();
                s_hook_ativo = nullptr;

                LOG_IF(LogSerial, "finalizando leitura");

                SERIAL_ECHOLNPGM("ok");
            } else {
                hook.buffer[hook.buffer_size++] = SERIAL_IMPL.read();
                if (hook.buffer_size >= sizeof(hook.buffer)) {
                    LOG_ERR("buffer nao tem espaco o suficiente!!");
                    hook.reset(true);
                    s_hook_ativo = nullptr;
                    break;
                }
                if (++hook.counter >= MAX_BYTES) {
                    hook.counter = 0;
                    SERIAL_ECHOLNPGM("ok");
                }
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
