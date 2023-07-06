#pragma once

#include <functional>

namespace lucas::util {
template<typename FN>
class ScopedGuard {
public:
    ScopedGuard(FN fn)
        : m_fn(std::move(fn)) {}
    ~ScopedGuard() { std::invoke(m_fn); }

private:
    FN m_fn;
};

template<typename FN>
ScopedGuard(FN&& fn) -> ScopedGuard<FN>;
}
