#pragma once

#include <array>
#include <lucas/util/util.h>
#include <lucas/serial/Hook.h>

namespace lucas::serial {
class DelimitedHook : public Hook {
public:
    static void think();

    static void make(char delimiter, Hook::Callback callback);

    static void for_each(util::IterFn<DelimitedHook&> auto&& callback) {
        if (not s_hooks_size)
            return;

        for (usize i = 0; i < s_hooks_size; ++i)
            if (std::invoke(callback), s_hooks[i])
                == util::Iter::Break break;
    }

    char delimiter() const { return m_delimiter; }

    void begin() { m_counter = 1; }

    // isso aqui poderia ser um std::vector mas nao vale a pena pagar o preço de alocar
    using List = std::array<DelimitedHook, 2>;

private:
    char m_delimiter = 0;

    static List s_hooks;
    static inline usize s_hooks_size = 0;
    DelimitedHook() = default;
};

}
