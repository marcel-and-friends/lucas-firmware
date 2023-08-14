#pragma once

#include <array>
#include <lucas/util/util.h>
#include <lucas/serial/Hook.h>

namespace lucas::serial {
class HookDelimitado : public Hook {
public:
    static void make(char delimitador, Hook::Callback callback);

    static void for_each(util::IterFn<HookDelimitado&> auto&& callback) {
        if (not s_num_hooks)
            return;

        for (size_t i = 0; i < s_num_hooks; ++i)
            if (std::invoke(FWD(callback), s_hooks[i]) == util::Iter::Break)
                break;
    }

    char delimitador() const { return m_delimitador; }

    void begin() { m_counter = 1; }

    // isso aqui poderia ser um std::vector mas nao vale a pena pagar o preço de alocar
    using Lista = std::array<HookDelimitado, 1>;

private:
    char m_delimitador = 0;

    static Lista s_hooks;
    static inline size_t s_num_hooks = 0;
    HookDelimitado() = default;
};

}
