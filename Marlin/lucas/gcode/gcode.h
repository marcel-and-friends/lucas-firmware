#pragma once

#include <string_view>

namespace lucas::gcode {

// Gcode executado ao apertar os botões
// (lembrando que as coordenadas são relativas!)
static constexpr auto RECEITA_PADRAO =
    R"(G3 F5000 I20 J20
G3 F5000 I15 J15
G3 F5000 I10 J10
G3 F5000 I5 J5
L0
G3 F5000 I5 J5
G3 F5000 I10 J10
G3 F5000 I15 J15
G3 F5000 I20 J20)";

// Gcodes necessário para o funcionamento ideal da máquina, executando quando a máquina liga, logo após conectar ao WiFi
static constexpr auto ROTINA_INICIAL =
    R"(G28 X Y
M190 R93)";

void injetar(std::string_view gcode);

std::string_view proxima_instrucao(std::string_view gcode);

bool lidar_com_custom_gcode(std::string_view gcode);

bool e_ultima_instrucao(std::string_view gcode);

void parar_fila();

bool tem_comandos_pendentes();

}
