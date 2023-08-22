#pragma once

namespace lucas {
class Boiler {
public:
    static Boiler& the() {
        static Boiler the;
        return the;
    }

    void setup();

    void tick();

    float hysteresis() const { return m_hysteresis; }
    void set_hysteresis(float f) { m_hysteresis = f; }

    void set_target_temperature(int);
    void set_target_temperature_and_wait(int);

    void disable_heater();

private:
    static constexpr auto INITIAL_HYSTERESIS = 1.5f;
    static constexpr auto FINAL_HYSTERESIS = 0.5f;

    float m_hysteresis;
};
}
