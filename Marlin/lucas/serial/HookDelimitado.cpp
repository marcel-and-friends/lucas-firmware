#include "HookDelimitado.h"
#include <lucas/lucas.h>

namespace lucas::serial {
HookDelimitado::Lista HookDelimitado::s_hooks = {};

void HookDelimitado::make(char delimitador, HookCallback callback) {
    if (index_atual >= s_hooks.size() - 1) {
        LOG("muitos hooks!! aumenta a capacidade ou diminui a quantidade");
        return;
    }

    auto& hook = s_hooks[index_atual++];
    hook.delimitador = delimitador;
    hook.callback = callback;
}
}
