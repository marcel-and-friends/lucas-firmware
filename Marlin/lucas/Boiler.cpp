#include "Boiler.h"

#include <lucas/lucas.h>
#include <src/module/temperature.h>

namespace lucas {
void Boiler::aguardar_temperatura(float target) {
    setar_temperatura(target);

    util::aguardar_enquanto([this, target] {
        constexpr auto INTERVALO_LOG = 5000;
        static auto ultimo_log = millis();
        if (millis() - ultimo_log >= INTERVALO_LOG) {
            LOG_IF(LogNivelamento, "info - [temp = ", thermalManager.degBed(), " | target = ", thermalManager.degTargetBed(), "]");
            ultimo_log = millis();
        }

        const auto delta = std::abs(target - thermalManager.degBed());
        return delta >= m_hysteresis;
    });

    LOG_IF(LogNivelamento, "temperatura desejada foi atingida");

    m_hysteresis = HYSTERESIS_FINAL;
}

void Boiler::setar_temperatura(float target) {
    thermalManager.setTargetBed(target);
    auto const temp_boiler = thermalManager.degBed();
    if (temp_boiler >= target)
        return;

    auto const delta = std::abs(target - temp_boiler);
    m_hysteresis = delta < HYSTERESIS_INICIAL ? HYSTERESIS_FINAL : HYSTERESIS_INICIAL;

    LOG_IF(LogNivelamento, "temperatura target setada - [target = ", target, " | hysteresis = ", m_hysteresis, "]");
}

}
