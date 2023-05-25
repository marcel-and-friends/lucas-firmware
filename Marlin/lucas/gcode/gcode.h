#pragma once

#include <lucas/lucas.h>
#include <string_view>
#include <avr/dtostrf.h>
#include <string>
#include <src/gcode/gcode.h>
#include <src/gcode/parser.h>

namespace lucas::gcode {
static constexpr auto RECEITA_PADRAO =
    R"(L3 D8 N3 R1
L0
L3 D6.5 N3 R1
L3 D4 N2 R2)";

constexpr size_t RECEITA_PADRAO_ID = 0xF0DA;

constexpr auto RECEITA_PADRAO_CABECALHO =
    R"($infoAtaque0duracao:25000
$infoAtaque0Intervalo:25000)";

void injetar(const char* gcode);

std::string_view proxima_instrucao(const char* gcode);

bool ultima_instrucao(const char* gcode);

void parar_fila();

bool tem_comandos_pendentes();

inline void executar(const char* gcode) {
    parser.parse(const_cast<char*>(gcode));
    GcodeSuite::process_parsed_command(true);
}

inline void executarf(const char* fmt, auto... args) {
    static char buffer[128] = {};
    sprintf(buffer, fmt, args...);
    executar(buffer);
}

inline void executarff(const char* fmt, float value) {
    static char buffer[16] = {};
    dtostrf(value, 0, 2, buffer);
    executarf(fmt, buffer);
}

// L0 -> Pausa a estação ativa e aguarda input do usuário
void L0();
// L1 -> Controla o bico
// P - Potência, [0-100], controla a força que a água é despejada
// T - Tempo, em milisegundos, que o bico deve ficar ligado
void L1();
// L2 -> Pausa cronometrada
// T - Tempo, em milisegundos, que a estação ativa vai ficar pausada
void L2();
// L3 -> Espiral
// D - Diâmetro máximo do circulo gerado
// N - Número de espirais dentro desse diâmetro
// R - Quantidade de vezes que o movimento deve ser repetido
// B - Iniciar na borda
void L3();
// L4 -> Enviar receita padrão para a primeira boca livre
void L4();
// L5 -> Viajar para uma estacao (pelo numero)
// N - Número da estação
void L5();
}
