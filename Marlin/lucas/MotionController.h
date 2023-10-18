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

    float step_ratio_x() const;

    float step_ratio_y() const;

private:
    static constexpr auto INVALID_LOCATION = static_cast<usize>(-1);
    static constexpr auto SEWER_LOCATION = Station::MAXIMUM_NUMBER_OF_STATIONS + 1;

    usize m_current_location = INVALID_LOCATION;
};
}
