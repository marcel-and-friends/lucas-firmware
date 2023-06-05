#include "HookDelimitado.h"

namespace lucas::serial {
HookDelimitado::Lista HookDelimitado::s_hooks = {};

void HookDelimitado::make(char delimitador, HookCallback callback) {
    HookDelimitado hook;
    hook.delimitador = delimitador;
    hook.callback = std::move(callback);
    hook.buffer.reserve(256);

    static size_t index = 0;
    s_hooks[index++] = std::move(hook);
}
}
