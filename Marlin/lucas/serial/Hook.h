#pragma once

#include <src/core/serial.h>
#include <lucas/types.h>
#include <span>

namespace lucas::serial {
class Hook {
public:
    static constexpr usize MAX_BUFFER_SIZE = 1024;
    static constexpr usize MAX_BUFFER_SLICE = 64;

    using Callback = void (*)(std::span<char>);

    void dispatch();

    void add_to_buffer(char c);

    bool is_valid() const;

public:
    usize buffer_size() const {
        return m_buffer_size;
    }

protected:
    void reset();

    void ok_to_receive();

protected:
    Callback m_callback = nullptr;

    usize m_counter = 0;

    char m_buffer[MAX_BUFFER_SIZE] = {};

    usize m_buffer_size = 0;
};
}
