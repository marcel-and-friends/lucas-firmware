#pragma once

#include <src/MarlinCore.h>

namespace lucas {

class Bico {
public:
    static void agir(millis_t tick);

    static void ativar(millis_t tick, millis_t tempo, float poder);

    static void reset();

    static void setup();

    static bool ativo() {
        return s_ativo;
    }

    static void set_min(float f) {
        s_min = f;
    }

    static void set_max(float f) {
        s_max = f;
    }

private:
    static inline float s_min = 0.f;

    static inline float s_max = 255.f;

    static inline millis_t s_tempo = 0;

    static inline millis_t s_tick = 0;

    static inline int s_poder = 0;

    static inline bool s_ativo = false;
};

}
