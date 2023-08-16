#include "DelimitedHook.h"
#include <lucas/lucas.h>

namespace lucas::serial {
DelimitedHook::List DelimitedHook::s_hooks = {};

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
