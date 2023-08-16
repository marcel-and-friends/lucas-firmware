#pragma once

#include <array>
#include <cstddef>
#include <utility>
#include <src/MarlinCore.h>

namespace lucas::util {
template<typename T, size_t Size>
class StaticVector {
public:
    void push_back(T value) {
        if (m_size >= Size) {
            LOG_ERR("StaticVector overflow");
            kill();
        }
        m_array[m_size++] = value;
    }

    bool is_empty() const { return m_size > 0; }

    decltype(auto) begin() { return m_array.begin(); }
    decltype(auto) begin() const { return m_array.begin(); }
    decltype(auto) end() { return std::next(m_array.begin(), m_size); }
    decltype(auto) end() const { return std::next(m_array.begin(), m_size); }

    size_t size() const { return m_size; }

private:
    std::array<T, Size> m_array = {};
    size_t m_size = 0;
};
}
