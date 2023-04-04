#include "gcode.h"
#include <cstring>
#include <src/gcode/parser.h>
#include <kofy/Boca.h>

namespace kofy::gcode {

void injetar(std::string_view gcode) {
	if (!queue.injected_commands_P)
		queue.injected_commands_P = gcode.data();

	if (lidar_com_custom_gcode(queue.injected_commands_P)) {
		auto fim_do_proximo_gcode = strchr(queue.injected_commands_P, '\n');
		if (fim_do_proximo_gcode != nullptr) {
			if (queue.injected_commands_P) {
				auto offset = fim_do_proximo_gcode - queue.injected_commands_P;
				queue.injected_commands_P += offset + 1;
			}
		}
	}
}

std::string_view proxima_instrucao(std::string_view gcode) {
	auto c_str = gcode.data();
    // como uma sequência de gcodes é separada pelo caractere de nova linha basta procurar um desse para encontrar o fim
    auto fim_do_proximo_gcode = strchr(c_str, '\n');
    if (!fim_do_proximo_gcode) {
        // se a nova linha não existe então estamos no último
        return gcode;
    }

    return std::string_view{ c_str, static_cast<size_t>(fim_do_proximo_gcode - c_str)};
}

bool lidar_com_custom_gcode(std::string_view gcode) {
	parser.parse(const_cast<char*>(gcode.data()));
	if (parser.command_letter != 'K')
		return false;

	switch (parser.codenum) {
		case 0:
			if (Boca::boca_ativa())
				Boca::boca_ativa()->aguardar_botao();

			parar_fila();

			break;
		case 1:
			Bico::ativar(millis(), parser.longval('T'), parser.longval('P'));
			Bico::debug();

			parar_fila();

			break;
	}

	DBG("executando 'K", parser.codenum, "'");

	return true;

}

bool ultima_instrucao(std::string_view gcode) {
	return strchr(gcode.data(), '\n') == nullptr;
}

void parar_fila() {
	queue.injected_commands_P = nullptr;
}

bool comandos_pendentes() {
	return queue.injected_commands_P != nullptr;
}

}
