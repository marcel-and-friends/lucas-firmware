#pragma once

#include <lucas/lucas.h>
#include <lucas/util/util.h>
#include <string_view>
#include <avr/dtostrf.h>
#include <string>
#include <src/gcode/gcode.h>
#include <src/gcode/parser.h>

namespace lucas::gcode {
static constexpr auto RECEITA_PADRAO =
    R"(L1 G1560 T6000
L3 F10250 D9 N3 R1
L0
L1 G1560 T6000
L3 F8250 D7 N3 R1
L2 T24000
L1 G1560 T9000
L3 F8500 D7 N5 R1
L2 T30000
L1 G1560 T10000
L3 F7650 D7 N5 R1
L2 T35000
L1 G1560 T10000
L3 F7650 D7 N5 R1)";

constexpr size_t RECEITA_PADRAO_ID = 0xF0DA;

void injetar(const char* gcode);

std::string_view proxima_instrucao(const char* gcode);

bool ultima_instrucao(const char* gcode);

void parar_fila();

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
