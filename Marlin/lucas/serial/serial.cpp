#include "serial.h"
#include <lucas/lucas.h>
#include <lucas/serial/HookDelimitado.h>
#include <src/core/serial.h>

namespace lucas::serial {
bool hooks() {
    constexpr auto MAX_BYTES = 64;

    static HookDelimitado* hook_ativo = nullptr;
    while (SERIAL_IMPL.available()) {
        const auto peek = SERIAL_IMPL.peek();
        if (hook_ativo) {
            auto& hook = *hook_ativo;
            if (hook.delimitador == peek) {
                hook.buffer[hook.buffer_size] = 0;
                SERIAL_IMPL.read();
                if (hook.callback && hook.buffer_size)
                    hook.callback({ hook.buffer, hook.buffer_size });

                hook.reset();
                LOG("ok");
                hook_ativo = nullptr;
                break;
            } else {
                hook.buffer[hook.buffer_size++] = SERIAL_IMPL.read();
                if (++hook.counter >= MAX_BYTES) {
                    hook.counter = 0;
                    LOG("ok");
                }
            }
        } else {
            HookDelimitado::for_each([peek](auto& hook) {
                if (hook.delimitador == peek) {
                    SERIAL_IMPL.read();
                    hook_ativo = &hook;
                    hook_ativo->counter = 1;
                    return util::Iter::Break;
                }
                return util::Iter::Continue;
            });
        }
        if (!hook_ativo)
            break;
    }
    return hook_ativo;
}
}
