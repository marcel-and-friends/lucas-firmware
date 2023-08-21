#include "DelimitedHook.h"
#include <lucas/lucas.h>

namespace lucas::serial {
DelimitedHook::List DelimitedHook::s_hooks = {};

void DelimitedHook::think() {
    static DelimitedHook* s_active_hook = nullptr;
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

                s_active_hook = nullptr;
                hook.dispatch();

                LOG_IF(LogSerial, "finalizando leitura");
            } else {
                hook.add_to_buffer(SERIAL_IMPL.read());
            }
        }

        if (not s_active_hook)
            return;
    }
}

void DelimitedHook::make(char delimiter, Hook::Callback callback) {
    if (s_hooks_size >= s_hooks.size()) {
        LOG_ERR("muitos hooks!!");
        return;
    }

    auto& hook = s_hooks[s_hooks_size++];
    hook.m_delimiter = delimiter;
    hook.m_callback = callback;
}
}
