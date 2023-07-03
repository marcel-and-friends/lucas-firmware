#pragma once

#include <cstdint>

namespace lucas::util {
template<size_t N>
struct FixedString {
    char __str_buf[N + 1]{};
    constexpr FixedString(const char* s) {
        for (size_t i = 0; i != N; ++i)
            __str_buf[i] = s[i];
    }
    constexpr operator const char*() const { return __str_buf; }

    auto operator<=>(const FixedString&) const = default;
};

template<size_t N>
FixedString(const char (&)[N]) -> FixedString<N - 1>;
}
