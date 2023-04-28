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
    lidar_com_gcode_customizado(queue.injected_commands_P);

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
#else
    queue.injected_commands_P = gcode.data();
    lidar_com_gcode_customizado(gcode);
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
    char cmd[gcode.size() + 1];
    memcpy_P(cmd, gcode.data(), gcode.size());
    cmd[gcode.size()] = '\0';
    parser.parse(cmd);
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
        const auto tick = millis();
        const auto tempo = parser.longval('T');
        const auto potencia = static_cast<float>(parser.longval('P')); // PWM funciona de [0-255], então multiplicamos antes de passar pro Bico
        Bico::ativar(tick, tempo, potencia);
        if (Estacao::ativa())
            parar_fila();

        break;
    }
    case 2: {
        if (!Estacao::ativa())
            break;

        const auto tempo = parser.longval('T');
        auto& estacao = *Estacao::ativa();
        estacao.pausar(tempo);
        Estacao::procurar_nova_ativa();

        break;
    }
    case 3: {
        static std::string buffer(512, '\0');
        buffer.clear();

        // o diametro é passado em cm, porem o marlin trabalho com mm
        const auto diametro = parser.floatval('D') * 10.f;
        const auto raio = diametro / 2.f;
        const auto num_circulos = parser.intval('N');
        const auto num_semicirculos = num_circulos * 2;
        const auto offset_por_semicirculo = raio / static_cast<float>(num_circulos);
        const auto repeticoes = parser.intval('R', 0);
        const auto series = repeticoes + 1;
        const auto comecar_na_borda = parser.seen_test('B');

        auto adicionar_gcode = [](const char* fmt, auto... args) {
            static char buffer_gcode[128] = {};
            sprintf(buffer_gcode, fmt, args...);
            buffer += buffer_gcode;
        };

        if (comecar_na_borda) {
            static char buffer_raio[16] = {};
            dtostrf(-raio, 0, 2, buffer_raio);
            adicionar_gcode("G0 F50000 X%s\n", buffer_raio);
        }

        for (auto i = 0; i < series; i++) {
            const bool fora_pra_dentro = i % 2 != comecar_na_borda;
            for (auto j = 0; j < num_semicirculos; j++) {
                const auto offset_semicirculo = offset_por_semicirculo * j;
                float offset_x = offset_semicirculo;
                if (fora_pra_dentro)
                    offset_x = fabs(offset_x - diametro);
                else
                    offset_x += offset_por_semicirculo;

                if (j % 2)
                    offset_x = -offset_x;

                const auto offset_centro = offset_x / 2;

                static char buffer_offset_x[16] = {};
                dtostrf(offset_x, 0, 2, buffer_offset_x);

                static char buffer_offset_centro[16] = {};
                dtostrf(offset_centro, 0, 2, buffer_offset_centro);

                adicionar_gcode("G2 F10000 X%s I%s\n", buffer_offset_x, buffer_offset_centro);
            }
        }

        if (series % 2 != comecar_na_borda)
            adicionar_gcode("G90\nG0 F50000 X%d Y63\nG91", Estacao::ativa()->posicao_absoluta());

        LOG("buffer = ", buffer.c_str());

        roubar_fila(buffer.c_str());
    }
    default:
        break;
    }
}

bool e_ultima_instrucao(std::string_view gcode) {
    return strchr(gcode.data(), '\n') == nullptr;
}

static const char* g_fila_roubada = nullptr;

void parar_fila() {
    queue.injected_commands_P = g_fila_roubada = nullptr;
}

void roubar_fila(const char* gcode) {
    g_fila_roubada = gcode;
    queue.injected_commands_P = g_fila_roubada;
}

bool roubando_fila() {
    return g_fila_roubada;
}

bool tem_comandos_pendentes() {
    return queue.injected_commands_P != nullptr;
}
}
