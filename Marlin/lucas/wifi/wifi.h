#pragma once

#include <string_view>

namespace lucas::wifi {
void conectar(std::string_view nome_rede, std::string_view senha_rede);

bool conectado();

void terminou_de_conectar();

std::string_view ip();

std::string_view nome_rede();

std::string_view senha_rede();
}
