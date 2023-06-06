#pragma once

#include <array>
#include <string>
#include <functional>
#include <span>
#include <lucas/util/util.h>

namespace lucas::serial {
using HookCallback = void (*)(std::span<char>);

struct HookDelimitado {
    static void make(char delimitador, HookCallback callback);

    template<typename FN>
    static void for_each(FN&& callback) {
        for (size_t i = 0; i < index_atual; ++i)
            if (std::invoke(std::forward<FN>(callback), s_hooks[i]) == util::Iter::Stop)
                break;
    }

    static constexpr inline size_t BUFFER_SIZE = 1024;
    static inline size_t index_atual = 0;

public:
    void reset() {
        counter = 0;
        buffer_size = 0;
    }

    size_t counter = 0;
    HookCallback callback = nullptr;
    char buffer[BUFFER_SIZE] = {};
    size_t buffer_size = 0;
    char delimitador = 0;

    // isso aqui poderia ser um std::vector mas nao vale a pena pagar o preço de alocar
    using Lista = std::array<HookDelimitado, 4>;

private:
    static Lista s_hooks;
    HookDelimitado() = default;
};

}
