#pragma once

#include <src/MarlinCore.h>

namespace lucas::util {

inline bool apertado(int pino) {
    return READ(pino) == false;
}

}
