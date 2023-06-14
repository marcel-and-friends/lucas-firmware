#pragma once

#include <src/MarlinCore.h>

namespace lucas::util {
inline const char* fmt(const char* fmt, auto... args) {
    static char buffer[128] = {};
    sprintf(buffer, fmt, args...);
    return buffer;
}

// isso aqui é uma desgraça mas é o que tem pra hoje
inline const char* ff(const char* str, float valor) {
    static char buffer[16] = {};
    dtostrf(valor, 0, 2, buffer);
    return fmt(str, buffer);
}

inline bool segurando(int pino) {
    return READ(pino) == false;
}

enum class Iter {
    Continue = 0,
    Stop
};

template<typename FN, typename... Args>
concept IterCallback = std::invocable<FN, Args...> && std::same_as<std::invoke_result_t<FN, Args...>, Iter>;

#define FWD(x) std::forward<decltype(x)>(x)
}
