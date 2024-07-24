#pragma once

#include <lucas/util/Singleton.h>
#include <lucas/Station.h>
#include <cstddef>

namespace lucas {
class MotionController : public util::Singleton<MotionController> {
public:
    void setup();

    void travel_to_station(const Station&, float offset = 0.f);

    void travel_to_station(usize, float offset = 0.f);

    void finish_movements() const;

    void travel_to_sewer();

    void home();

    float step_ratio_x() const;

    float step_ratio_y() const;

    void change_step_ratio(f32 ratio) const;

    void change_step_ratio(f32 ratio_x, f32 ratio_y) const;

    void change_max_acceleration(f32 accel) const;

    void invalidate_location() { m_current_location = INVALID_LOCATION; }

    void toggle_motors_stress_test();

    static inline float MS_PER_MM = 12.41f;
    static constexpr float DEFAULT_STEPS_PER_MM_X = 22.0f;
    static constexpr float DEFAULT_STEPS_PER_MM_Y = 10.0f;
    static constexpr float ANGLE_FIX = 1.2f;

private:
    static constexpr auto INVALID_LOCATION = static_cast<usize>(-1);
    static constexpr auto SEWER_LOCATION = Station::MAXIMUM_NUMBER_OF_STATIONS + 1;

    usize m_current_location = INVALID_LOCATION;

    bool m_motor_stress_test = false;
};
}
