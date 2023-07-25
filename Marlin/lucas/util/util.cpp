#include <lucas/util/util.h>
#include <src/MarlinCore.h>
#include <src/module/planner.h>
#include <avr/dtostrf.h>

namespace lucas::util {
// isso aqui é uma desgraça mas é o que tem pra hoje
const char* ff(const char* str, float valor) {
    char buffer[16] = {};
    dtostrf(valor, 0, 2, buffer);
    return fmt(str, buffer);
}

bool segurando(int pino) {
    return READ(pino) == false;
}

float step_ratio_x() {
    return planner.settings.axis_steps_per_mm[X_AXIS] / DEFAULT_STEPS_POR_MM_X;
}

float step_ratio_y() {
    return planner.settings.axis_steps_per_mm[Y_AXIS] / DEFAULT_STEPS_POR_MM_Y;
}

float distancia_primeira_estacao() {
    return 80.f / step_ratio_x();
}

float distancia_entre_estacoes() {
    return 160.f / step_ratio_x();
}

static float s_hysteresis = 0.5f;

float hysteresis() {
    return s_hysteresis;
}

void set_hysteresis(float valor) {
    s_hysteresis = valor;
}

void aguardar_por(millis_t tempo, Filtros filtros) {
    FiltroUpdatesTemporario f{ filtros };

    const auto comeco = millis();
    while (millis() - comeco < tempo)
        idle();
}
}
