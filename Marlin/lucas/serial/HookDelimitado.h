#pragma once

#include <array>
#include <string>
#include <functional>
#include <span>
#include <lucas/util/util.h>

namespace lucas::serial {
using HookCallback = std::function<void(std::span<const char>)>;

struct HookDelimitado {
    static void make(char delimitador, HookCallback callback);

    static void for_each(auto&& callback) {
        for (auto& hook : s_hooks)
            if (std::invoke(callback, hook) == util::Iter::Stop)
                break;
    }

    void reset() {
        counter = 0;
        buffer.clear();
    }

    HookDelimitado(HookDelimitado&&) = default;
    HookDelimitado& operator=(HookDelimitado&&) = default;

    size_t counter = 0;
    HookCallback callback = nullptr;
    std::string buffer = "";
    char delimitador = 0;

    // isso aqui poderia ser um std::vector mas nao vale a pena pagar o preço de alocar
    using Lista = std::array<HookDelimitado, 4>;

private:
    static Lista s_hooks;
    HookDelimitado() = default;
};

}
