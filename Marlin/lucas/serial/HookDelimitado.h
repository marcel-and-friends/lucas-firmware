#pragma once

#include <array>
#include <string>
#include <functional>
#include <span>
#include <lucas/util/util.h>
#include <lucas/info/info.h>

namespace lucas::serial {
using HookCallback = void (*)(std::span<char>);

struct HookDelimitado {
    static void make(char delimitador, HookCallback callback);

    static void for_each(auto&& callback) {
        if (!s_num_hooks)
            return;

        for (size_t i = 0; i < s_num_hooks; ++i)
            if (std::invoke(FWD(callback), s_hooks[i]) == util::Iter::Break)
                break;
    }

public:
    void reset() {
        counter = 0;
        buffer_size = 0;
    }

    size_t counter = 0;
    HookCallback callback = nullptr;
    char buffer[info::BUFFER_SIZE] = {};
    size_t buffer_size = 0;
    char delimitador = 0;

    // isso aqui poderia ser um std::vector mas nao vale a pena pagar o preço de alocar
    using Lista = std::array<HookDelimitado, 1>;

private:
    static Lista s_hooks;
    static inline size_t s_num_hooks = 0;
    HookDelimitado() = default;
};

}
