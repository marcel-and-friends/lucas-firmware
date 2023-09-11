#pragma once

#include <array>
#include <cstddef>
#include <utility>
#include <src/MarlinCore.h>

namespace lucas::util {
template<typename T, usize Size>
class StaticVector {
public:
    using Storage = std::array<T, Size>;

    void push_back(T value) {
        m_storage[m_size++] = std::move(value);
    }

    template<typename... Args>
    void emplace_back(Args&&... args) {
        new (&m_storage[m_size++]) T(std::forward<Args>(args)...);
    }

    void pop_back() {
        m_storage[--m_size].~T();
    }

    void clear() {
        m_size = 0;
    }

    bool is_empty() const {
        return m_size == 0;
    }

    Storage::const_iterator min() const {
        return std::min_element(begin(), end());
    }

    Storage::const_iterator max() const {
        return std::max_element(begin(), end());
    }

    T& operator[](usize index) { return m_storage[index]; }
    const T& operator[](usize index) const { return m_storage[index]; }

    Storage::iterator begin() { return m_storage.begin(); }
    Storage::const_iterator begin() const { return m_storage.begin(); }
    Storage::iterator end() { return std::next(m_storage.begin(), m_size); }
    Storage::const_iterator end() const { return std::next(m_storage.begin(), m_size); }

    usize size() const { return m_size; }

private:
    Storage m_storage;
    usize m_size = 0;
};
}
