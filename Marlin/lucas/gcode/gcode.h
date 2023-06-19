#pragma once

#include <lucas/lucas.h>
#include <string_view>
#include <src/gcode/gcode.h>
#include <src/gcode/parser.h>

namespace lucas::cmd {
void injetar(const char* gcode);

std::string_view proxima_instrucao(const char* gcode);

bool ultima_instrucao(const char* gcode);

void parar_fila();

bool comandos_pendentes();

inline void executar(const char* gcode) {
    parser.parse(const_cast<char*>(gcode));
    GcodeSuite::process_parsed_command(true);
}

inline void executar_cmds(auto... cmds) {
    (executar(cmds), ...);
}

inline void executar_fmt(const char* str, auto... args) {
    executar(util::fmt(str, args...));
}

inline void executar_ff(const char* str, float value) {
    executar_fmt(util::ff(str, value));
}

// L0 -> Pausa a estação ativa e aguarda input do usuário
void L0();
// L1 -> Controla o bico
// G - Fluxo de água em g/s
// T - Tempo, em milisegundos, que o bico deve ficar ligado
void L1();
// L2 -> Pausa cronometrada
// T - Tempo, em milisegundos, que a estação ativa vai ficar pausada
void L2();
// L3 -> Espiral
// D - Diâmetro máximo do circulo gerado
// N - Número de espirais dentro desse diâmetro
// [R] - Quantidade de vezes que o movimento deve ser repetido
// [B] - Iniciar na borda
void L3();
// L4 -> Enviar receita padrão para a primeira boca livre
void L4();
// L5 -> Viajar para uma estacao (pelo numero)
// N - Número da estação
void L5();
}
