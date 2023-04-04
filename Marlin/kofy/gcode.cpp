#include "gcode.h"
#include <cstring>
#include <src/gcode/parser.h>
#include <kofy/Boca.h>

namespace kofy::gcode {

void injetar(std::string_view gcode) {
	if (!queue.injected_commands_P)
		queue.injected_commands_P = gcode.data();

	auto proxima = gcode::proxima_instrucao(queue.injected_commands_P);
	if (lidar_com_gcode_custom(queue.injected_commands_P)) {
		auto fim_do_proximo_gcode = strchr(queue.injected_commands_P, '\n');
		if (fim_do_proximo_gcode == nullptr) {
			queue.injected_commands_P = nullptr;
		} else {
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

bool lidar_com_gcode_custom(std::string_view gcode) {
	parser.parse(const_cast<char*>(gcode.data()));
	if (parser.command_letter != 'K')
		return false;

	switch (parser.codenum) {
		case 0:
			if (Boca::boca_ativa())
				Boca::boca_ativa()->aguardar_botao();

			marlin::parar_fila_de_gcode();
			break;
		case 1:
			g_bico.ativo = true;
			g_bico.tempo = parser.ulongval('T');
			g_bico.poder = parser.longval('P');
			g_bico.tick = millis();

			DBG("T: ", g_bico.tempo, " - P: ", g_bico.poder, " - tick: ", g_bico.tick);
			marlin::parar_fila_de_gcode();
			break;
	}

	if (Boca::boca_ativa())
		DBG("executando 'K", parser.codenum, "'");

	return true;

}

bool ultima_instrucao(std::string_view gcode) {
	return strchr(gcode.data(), '\n') == nullptr;
}

}
