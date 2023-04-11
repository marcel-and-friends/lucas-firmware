#pragma once

#include <string_view>

namespace kofy::wifi {
	void conectar(std::string_view nome_rede, std::string_view senha_rede);

	bool conectado();

	std::string_view ip();

	std::string_view nome_rede();

	std::string_view senha_rede();
}
