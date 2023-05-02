#pragma once

#include <string_view>
#include <avr/dtostrf.h>
#include <string>

namespace lucas::gcode {

static constexpr auto RECEITA_PADRAO =
    R"(L3 D8 N3 R1
L0
L3 D6.5 N3 R1
L2 T10000
L3 D7 N5 R1
L2 T15000
L3 D7.5 N5 R1
L2 T20000
L3 D8 N5 R1)";

// Gcodes necessário para o funcionamento ideal da máquina, executando quando a máquina liga, logo após conectar ao WiFi
static constexpr auto ROTINA_INICIAL =
    R"(G28 X Y
M190 R93)";

void injetar(std::string_view gcode);

void injetar_imediato(const char* gcode);

std::string_view proxima_instrucao(std::string_view gcode);

bool e_ultima_instrucao(std::string_view gcode);

void parar_fila();

void roubar_fila(const char* gcode);

bool roubando_fila();

void fila_roubada_terminou();

bool tem_comandos_pendentes();

inline void adicionar_instrucao(std::string& output, const char* gcode, auto... args) {
    static char buffer_gcode[128] = {};
    sprintf(buffer_gcode, gcode, args...);
    output += buffer_gcode;
}

inline void adicionar_instrucao_float(std::string& output, const char* gcode, float valor, int prec = 2) {
    static char buffer_float[16] = {};
    dtostrf(valor, 0, prec, buffer_float);
    adicionar_instrucao(output, gcode, buffer_float);
}

// L0 -> Pausa a estação ativa e aguarda input do usuário
void L0();
// L1 -> Controla o bico
// T - Tempo, em milisegundos, que o bico deve ficar ligado
// P - Potência, [0-100], controla a força que a água é despejada
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
}
