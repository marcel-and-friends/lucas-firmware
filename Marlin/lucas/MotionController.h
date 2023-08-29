#pragma once

#include <lucas/util/Singleton.h>
#include <lucas/Station.h>
#include <cstddef>

namespace lucas {
class MotionController : public util::Singleton<MotionController> {
public:
    void travel_to_station(const Station&, float offset = 0.f);

    void travel_to_station(size_t, float offset = 0.f);

    void finish_movements() const;

    void travel_to_sewer();

    void home();

private:
    size_t m_current_station = Station::INVALID;
};
}
