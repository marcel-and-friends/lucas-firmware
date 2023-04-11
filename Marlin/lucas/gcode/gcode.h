#pragma once

#include <string_view>

namespace lucas::gcode {

void injetar(std::string_view gcode);

std::string_view proxima_instrucao(std::string_view gcode);

bool lidar_com_custom_gcode(std::string_view gcode);

bool e_ultima_instrucao(std::string_view gcode);

void parar_fila();

bool tem_comandos_pendentes();

}
