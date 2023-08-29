#pragma once

#include <lucas/serial/Hook.h>
#include <lucas/util/Singleton.h>

namespace lucas::serial {
class FirmwareUpdateHook : public Hook, public util::Singleton<FirmwareUpdateHook> {
public:
    void activate(Hook::Callback callback, size_t bytes_to_receive) {
        m_active = true;
        m_callback = callback;
        m_bytes_to_receive = bytes_to_receive;
        m_bytes_received = 0;
    }

    bool active() const { return m_active; }

    void think();

    void receive_char(char c);

private:
    bool m_active = false;
    size_t m_bytes_to_receive = 0;
    size_t m_bytes_received = 0;
};
}
