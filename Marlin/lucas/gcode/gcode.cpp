#include "gcode.h"
#include <cstring>
#include <src/gcode/parser.h>
#include <src/gcode/queue.h>
#include <lucas/Estacao.h>

namespace lucas::gcode {

void injetar(std::string_view gcode) {
#ifdef LUCAS_ROUBAR_FILA_GCODE
    if (!queue.injected_commands_P)
        queue.injected_commands_P = gcode.data();
#else
    queue.injected_commands_P = gcode.data();
#endif

    lidar_com_gcode_customizado(queue.injected_commands_P);

#ifdef LUCAS_ROUBAR_FILA_GCODE
    // se o próximo gcode é customizado e a fila não foi parada
    // nós temos de manualmente pula-lo para evitar erros de gcode desconhecido no serial
    if (queue.injected_commands_P) {
        auto fim_do_proximo_gcode = strchr(queue.injected_commands_P, '\n');
        if (fim_do_proximo_gcode) {
            auto offset = fim_do_proximo_gcode - queue.injected_commands_P;
            queue.injected_commands_P += offset + 1;
        } else {
            queue.injected_commands_P = nullptr;
        }
    }
#endif
}

std::string_view proxima_instrucao(std::string_view gcode) {
    auto c_str = gcode.data();
    // como uma sequência de gcodes é separada pelo caractere de nova linha basta procurar um desse para encontrar o fim
    auto fim_do_proximo_gcode = strchr(c_str, '\n');
    if (!fim_do_proximo_gcode) {
        // se a nova linha não existe então estamos no último
        return gcode;
    }

    return std::string_view{ c_str, static_cast<size_t>(fim_do_proximo_gcode - c_str) };
}

void lidar_com_gcode_customizado(std::string_view gcode) {
    parser.parse(const_cast<char*>(gcode.data()));
    if (parser.command_letter != 'L')
        return;

    LOG("lidando manualmente com gcode customizado 'L", parser.codenum, "'");

    switch (parser.codenum) {
    // L0 -> Interrompe a estacao ativa até seu botão ser apertado
    case 0: {
        if (!Estacao::ativa())
            break;

        Estacao::ativa()->aguardar_input();
        Estacao::procurar_nova_ativa();

        break;
    }
    // L1 -> Controla o bico
    // T - Tempo, em milisegundos, que o bico deve ficar ligado
    // P - Potência, [0-100], controla a força que a água é despejada
    case 1: {
        auto tick = millis();
        auto tempo = parser.longval('T');
        auto potencia = static_cast<float>(parser.longval('P')) * 0.16f; // PWM funciona de [0-255], então multiplicamos antes de passar pro Bico
        Bico::ativar(tick, tempo, potencia);
        if (Estacao::ativa())
            parar_fila();

        break;
    }
    case 2: {
        if (!Estacao::ativa())
            break;

        auto tempo = parser.longval('T');
        auto& estacao = *Estacao::ativa();
        estacao.pausar(tempo);
        Estacao::procurar_nova_ativa();

        break;
    }
    default: {
        break;
    }
    }
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
