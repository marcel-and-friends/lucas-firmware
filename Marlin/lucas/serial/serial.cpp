#include "serial.h"
#include <lucas/lucas.h>
#include <src/core/serial.h>

namespace lucas::serial {
void setup() {
    while (SERIAL_IMPL.available())
        SERIAL_IMPL.read();

    HookDelimitado::make(
        '#',
        [](std::span<char> buffer) {
            info::interpretar_json(buffer);
        });

    LOG("jujuba");
}

bool hooks() {
    constexpr auto MAX_BYTES = 64;
    static HookDelimitado* s_hook_ativo = nullptr;

    while (SERIAL_IMPL.available()) {
        const auto peek = SERIAL_IMPL.peek();
        if (!s_hook_ativo) {
            LOG_IF(LogSerial, "procurando hook - peek = ", AS_CHAR(peek));

            HookDelimitado::for_each([&](auto& hook) {
                if (hook.delimitador == peek) {
                    SERIAL_IMPL.read();

                    LOG_IF(LogSerial, "iniciando leitura");

                    s_hook_ativo = &hook;
                    s_hook_ativo->counter = 1;

                    return util::Iter::Break;
                }
                return util::Iter::Continue;
            });
        } else {
            auto& hook = *s_hook_ativo;
            if (hook.delimitador == peek) {
                SERIAL_IMPL.read();

                LOG_IF(LogSerial, "finalizando leitura");
                hook.buffer[hook.buffer_size] = '\0';
                if (hook.callback && hook.buffer_size) {
                    LOG_IF(LogSerial, "!BUFFER!\n", hook.buffer);
                    LOG_IF(LogSerial, "!BUFFER!");

                    hook.callback({ hook.buffer, hook.buffer_size });
                }

                hook.reset();
                s_hook_ativo = nullptr;

                LOG("ok");
            } else {
                hook.buffer[hook.buffer_size++] = SERIAL_IMPL.read();
                if (hook.buffer_size >= sizeof(hook.buffer)) {
                    LOG_ERR("buffer nao tem espaco o suficiente!!!");
                    hook.reset(true);
                    s_hook_ativo = nullptr;
                    break;
                }
                if (++hook.counter >= MAX_BYTES) {
                    hook.counter = 0;
                    LOG_IF(LogSerial, "limite de 64 atingido");
                    LOG("ok");
                }
            }
        }

        if (!s_hook_ativo)
            break;
    }
    return s_hook_ativo;
}
}
