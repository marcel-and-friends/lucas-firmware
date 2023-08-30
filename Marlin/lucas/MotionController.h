#pragma once

#include <lucas/util/Singleton.h>
#include <lucas/Station.h>
#include <cstddef>

namespace lucas {
class MotionController : public util::Singleton<MotionController> {
public:
    void travel_to_station(const Station&, float offset = 0.f);

    void travel_to_station(usize, float offset = 0.f);

    void finish_movements() const;

    void travel_to_sewer();

    void home();

private:
    usize m_current_station = Station::INVALID;
};
}
