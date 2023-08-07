#pragma once

namespace lucas {
class Boiler {
public:
    static Boiler& the() {
        static Boiler the;
        return the;
    }

    void setar_temperatura(float target);

    void aguardar_temperatura(float target);

    float hysteresis() const { return m_hysteresis; }
    void set_hysteresis(float f) { m_hysteresis = f; }

private:
    static constexpr auto HYSTERESIS_INICIAL = 1.5f;
    static constexpr auto HYSTERESIS_FINAL = 0.5f;

    float m_hysteresis;
};
}
