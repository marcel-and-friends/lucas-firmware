#include "HookDelimitado.h"
#include <lucas/lucas.h>

namespace lucas::serial {
HookDelimitado::Lista HookDelimitado::s_hooks = {};

void HookDelimitado::make(char delimitador, HookCallback callback) {
    if (s_num_hooks >= s_hooks.size()) {
        LOG_ERR("muitos hooks!!");
        return;
    }

    auto& hook = s_hooks[s_num_hooks++];
    hook.delimitador = delimitador;
    hook.callback = callback;
}
}
