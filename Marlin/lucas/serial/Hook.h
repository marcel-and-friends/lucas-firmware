#pragma once

#include <src/core/serial.h>
#include <span>

namespace lucas::serial {
class Hook {
public:
    static constexpr size_t MAX_BUFFER_SIZE = 1024;
    static constexpr size_t MAX_BUFFER_SLICE = 64;
    static constexpr auto OK_CODE = "ok";

    using Callback = void (*)(std::span<char>);

    void dispatch();

    void add_to_buffer(char c);

    bool is_valid() const;

public:
    size_t buffer_size() const {
        return m_buffer_size;
    }

protected:
    void reset();

    void ok_to_receive();

protected:
    Callback m_callback = nullptr;

    size_t m_counter = 0;

    char m_buffer[MAX_BUFFER_SIZE] = {};

    size_t m_buffer_size = 0;
};
}
