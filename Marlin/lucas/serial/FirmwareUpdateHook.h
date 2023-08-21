#pragma once

#include <lucas/serial/Hook.h>

namespace lucas::serial {
class FirmwareUpdateHook : public Hook {
public:
    static void create(Hook::Callback callback, size_t bytes_to_receive) {
        s_the.m_callback = callback;
        s_the.m_active = true;
        s_the.m_bytes_to_receive = bytes_to_receive;
        s_the.m_bytes_received = 0;
    }

    static FirmwareUpdateHook* the() {
        return s_the.m_active ? &s_the : nullptr;
    }

    static bool active() { return s_the.m_active; }

    void think();

    void receive_char(char c);

private:
    FirmwareUpdateHook() = default;
    static FirmwareUpdateHook s_the;

    bool m_active = false;
    size_t m_bytes_to_receive = 0;
    size_t m_bytes_received = 0;
};
}
