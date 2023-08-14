#pragma once

#include <lucas/serial/Hook.h>

namespace lucas::serial {
class UniquePriorityHook : public Hook {
public:
    static void set(Hook::Callback callback, size_t bytes_to_receive) {
        s_the.m_callback = callback;
        s_the.m_active = true;
        s_the.m_bytes_to_receive = bytes_to_receive;
        s_the.m_bytes_received = 0;
    }

    static void unset() {
        s_the.reset();
        s_the.m_callback = nullptr;
        s_the.m_active = false;
        s_the.m_bytes_to_receive = 0;
    }

    static UniquePriorityHook* the() {
        return s_the.m_active ? &s_the : nullptr;
    }

    void receive_char(char c);

private:
    UniquePriorityHook() = default;
    static UniquePriorityHook s_the;

    bool m_active = false;
    size_t m_bytes_to_receive = 0;
    size_t m_bytes_received = 0;
};
}
