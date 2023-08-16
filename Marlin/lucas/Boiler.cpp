#include "Boiler.h"

#include <lucas/lucas.h>
#include <src/module/temperature.h>

namespace lucas {
void Boiler::set_target_temperature_and_wait(float target) {
    set_target_temperature(target);
    auto const temp_boiler = thermalManager.degBed();
    if (temp_boiler >= target)
        return;

    util::wait_while([this, target] {
        constexpr auto LOG_INTERVAL = 5000;
        static auto last_log_tick = millis();
        if (millis() - last_log_tick >= LOG_INTERVAL) {
            LOG_IF(LogCalibration, "info - [temp = ", thermalManager.degBed(), " | target = ", thermalManager.degTargetBed(), "]");
            last_log_tick = millis();
        }

        const auto delta = std::abs(target - thermalManager.degBed());
        return delta >= m_hysteresis;
    });

    LOG_IF(LogCalibration, "temperatura desejada foi atingida");

    m_hysteresis = FINAL_HYSTERESIS;
}

void Boiler::set_target_temperature(float target) {
    thermalManager.setTargetBed(target);

    auto const delta = std::abs(target - thermalManager.degBed());
    m_hysteresis = delta < INITIAL_HYSTERESIS ? FINAL_HYSTERESIS : INITIAL_HYSTERESIS;

    LOG_IF(LogCalibration, "temperatura target setada - [target = ", target, " | hysteresis = ", m_hysteresis, "]");
}

}
