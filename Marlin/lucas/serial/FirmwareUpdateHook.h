#pragma once

#include <lucas/serial/Hook.h>
#include <lucas/util/Singleton.h>

namespace lucas::serial {
class FirmwareUpdateHook : public Hook, public util::Singleton<FirmwareUpdateHook> {
public:
    void activate(Hook::Callback callback, usize bytes_to_receive) {
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
    usize m_bytes_to_receive = 0;
    usize m_bytes_received = 0;
};
}
