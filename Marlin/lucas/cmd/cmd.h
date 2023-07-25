#pragma once

#include <lucas/lucas.h>
#include <string_view>
#include <src/gcode/gcode.h>
#include <src/gcode/parser.h>

namespace lucas::cmd {
void executar(const char* gcode);

inline void executar_cmds(auto... cmds) {
    (executar(cmds), ...);
}

inline void executar_fmt(const char* str, auto... args) {
    executar(util::fmt(str, args...));
}

inline void executar_ff(const char* str, float value) {
    executar(util::ff(str, value));
}

// L1 -> Controla o bico
// G - Volume total de água a ser despejado
// T - Tempo, em milisegundos, que o bico deve ficar ligado
void L1();
// L3 -> Espiral
// D - Diâmetro máximo do circulo gerado
// N - Número de espirais dentro desse diâmetro
// [G] - Volume de água a ser despejado durante a espiral (requer o parâmetro T)
// [T] - Tempo aproximado, em milisegundos, que o movimento e despejo vai durar
// [R] - Quantidade de vezes que o movimento deve ser repetido
// [B] - Iniciar na borda
void L3();
// L5 -> Viajar para uma estacao (pelo numero)
// N - Número da estação
void L5();
void L6();
void L7();
}
