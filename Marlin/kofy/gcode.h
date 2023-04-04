#pragma once
#include <string_view>

namespace kofy::gcode {

void injetar(std::string_view gcode);

std::string_view proxima_instrucao(std::string_view gcode);

bool lidar_com_gcode_custom(std::string_view gcode);

bool ultima_instrucao(std::string_view gcode);

}
