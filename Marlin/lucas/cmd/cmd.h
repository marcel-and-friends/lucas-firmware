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

// L0 -> Espiral
// D - Diâmetro máximo do circulo gerado
// N - Número de espirais dentro desse diâmetro
// [G] - Volume de água a ser despejado durante a espiral (requer o parâmetro T)
// [T] - Tempo aproximado, em milisegundos, que o movimento e despejo irão durar
// [R] - Quantidade de vezes que o movimento deve ser repetido
// [B] - Iniciar na borda
void L0();
// L1 -> Círculo
// D - Diâmetro máximo do circulo gerado
// N - Número de repetições
// [G] - Volume de água a ser despejado durante a espiral (requer o parâmetro T)
// [T] - Tempo aproximado, em milisegundos, que o movimento e despejo irão durar
void L1();
// L2 -> Despejo de água
// G - Volume total de água a ser despejado
// T - Tempo, em milisegundos, que o bico deve ficar ligado
void L2();
/* ~comandos de desenvolvimento~ */
void L3();
void L4();
}
