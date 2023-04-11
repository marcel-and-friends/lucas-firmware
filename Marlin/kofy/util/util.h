#pragma once

#include <src/MarlinCore.h>

namespace kofy::util {

inline bool apertado(int pino) {
    return READ(pino) == false;
}

}
