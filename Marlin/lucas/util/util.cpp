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

bool is_button_held(s32 pin) {
    return digitalRead(pin) == LOW;
}

float normalize(float v, float min, float max) {
    return (v - min) / (max - min);
}
}
