#include "util.h"
#include <src/module/planner.h>
#include <avr/dtostrf.h>

namespace lucas::util {
// isso aqui é uma desgraça mas é o que tem pra hoje
const char* ff(const char* str, float valor) {
    char buffer[16] = {};
    dtostrf(valor, 0, 2, buffer);
    return fmt(str, buffer);
}

bool is_button_held(int pin) {
    return READ(pin) == false;
}

float step_ratio_x() {
    return planner.settings.axis_steps_per_mm[X_AXIS] / DEFAULT_STEPS_PER_MM_X;
}

float step_ratio_y() {
    return planner.settings.axis_steps_per_mm[Y_AXIS] / DEFAULT_STEPS_PER_MM_Y;
}

float first_station_abs_pos() {
    return 80.f / step_ratio_x();
}

float distance_between_each_station() {
    return 160.f / step_ratio_x();
}

float normalize(float v, float min, float max) {
    return (v - min) / (max - min);
}
}
