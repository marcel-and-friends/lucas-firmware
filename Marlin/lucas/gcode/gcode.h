#pragma once

#include <string_view>

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

std::string_view proxima_instrucao(std::string_view gcode);

void lidar_com_gcode_customizado(std::string_view gcode);

bool e_ultima_instrucao(std::string_view gcode);

void parar_fila();

void roubar_fila(const char* gcode);

bool roubando_fila();

bool tem_comandos_pendentes();

}
