#include "gcode.h"
#include <cstring>
#include <src/gcode/parser.h>
#include <src/gcode/queue.h>
#include <lucas/Boca.h>

namespace lucas::gcode {

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
		// K0 -> Interrompe a boca ativa até seu botão ser apertado
		case 0:
			if (!Boca::ativa())
				break;

			Boca::ativa()->aguardar_botao();
			parar_fila();

			break;
		// K1 -> Controla o bico
		// T - Tempo, em milisegundos, que o bico deve ficar ligado
		// P - Potência, [0-100], controla a força que a água é despejada
		case 1: {
			auto tick = millis();
			auto tempo = parser.longval('T');
			// FIXME: fazer uma conta mais legal aqui pra trabalhar somente no range de tensão ideal
			//		  algo entre 2-5V fica bom, o pino atual (FAN) produz 12V se passamos 100 para o K1.
			auto potencia = parser.longval('P') * 2.5; // PWM funciona de [0-255], então multiplicamos antes de passar pro Bico
			Bico::ativar(tick, tempo, potencia);
			if (Boca::ativa())
				parar_fila();

			break;
		}
	}

	DBG("lidando manualmente com gcode customizado 'K", parser.codenum, "'");

	return true;

}

bool e_ultima_instrucao(std::string_view gcode) {
	return strchr(gcode.data(), '\n') == nullptr;
}

void parar_fila() {
	queue.injected_commands_P = nullptr;
}

bool tem_comandos_pendentes() {
	return queue.injected_commands_P != nullptr;
}

}
