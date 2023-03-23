#pragma once

#include <string_view>

namespace kofy::marlin::wifi {
	void conectar(std::string_view nome_rede, std::string_view senha_rede);

	bool conectado();
}