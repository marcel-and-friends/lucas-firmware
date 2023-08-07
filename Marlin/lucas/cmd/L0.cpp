#include <lucas/Estacao.h>
#include <lucas/Bico.h>
#include <lucas/Fila.h>
#include <src/gcode/gcode.h>
#include <src/gcode/parser.h>
#include <lucas/cmd/cmd.h>
#include <src/module/planner.h>
#include <numbers>

#define L0_LOG(...) LOG_IF(LogLn, "", "L0: ", __VA_ARGS__);

namespace lucas::cmd {
void L0() {
    // o diametro é passado em cm, porem o marlin trabalho com mm
    auto const diametro_total = (parser.floatval('D') / util::step_ratio_x()) * 10.f;
    if (diametro_total == 0.f)
        return;

    auto const raio = diametro_total / 2.f;
    auto const num_circulos = parser.intval('N');
    if (num_circulos == 0)
        return;

    // cada circulo é composto por 2 arcos
    auto const num_arcos = num_circulos * 2;
    auto const offset_por_arco = raio / static_cast<float>(num_circulos);
    auto const repeticoes = parser.intval('R', 0);
    auto const series = repeticoes + 1;
    auto const comecar_na_borda = parser.seen_test('B');
    auto const duracao = parser.ulongval('T');

    L0_LOG("!opcoes!\ndiametro = ", diametro_total, "\nraio = ", raio, "\nnum_circulos = ", num_circulos, "\nnum_arcos = ", num_arcos, "\noffset_por_arco = ", offset_por_arco, "\nrepeticoes = ", repeticoes, "\nseries = ", series, "\ncomecar_na_borda = ", comecar_na_borda, "\nduracao = ", duracao);

    if (CFG(ModoGiga) and duracao) {
        L0_LOG("iniciado no modo giga");
        util::aguardar_por(duracao);
        L0_LOG("finalizado no modo giga");
        return;
    }

    auto const volume_agua = parser.floatval('G');
    auto const despejar_agua = duracao and volume_agua;

    bool const associado_a_estacao = Fila::the().executando();
    bool vaza = false;

    auto const posicao_inicial = planner.get_axis_positions_mm();
    auto posicao_final = posicao_inicial;
    L0_LOG("posicao_inicial.x: ", posicao_inicial.x);

    std::vector<float> diametros;
    diametros.reserve(num_arcos * series);

    for (auto serie = 0; serie < series; serie++) {
        bool const fora_pra_dentro = serie % 2 != comecar_na_borda;
        for (auto arco = 0; arco < num_arcos; arco++) {
            auto const offset_arco = offset_por_arco * arco;
            float diametro_arco = fora_pra_dentro ? std::abs(offset_arco - diametro_total) : offset_arco + offset_por_arco;
            if (arco % 2)
                diametro_arco = -diametro_arco;

            diametros.emplace_back(diametro_arco);
        }
    }

    char buffer_raio[16] = {};
    dtostrf(raio, 0, 2, buffer_raio);

    if (comecar_na_borda) {
        executar_fmt("G0 F50000 X-%s", buffer_raio);
        Bico::the().aguardar_viagem_terminar();
    }

    float total_percorrido = 0.f;
    for (auto const diametro : diametros)
        total_percorrido += (2.f * std::numbers::pi_v<float> * std::abs(diametro / 2.f)) / 2.f;

    auto const steps_por_mm_ratio = duracao ? util::MS_POR_MM / (duracao / total_percorrido) : 1.f;

    soft_endstop._enabled = false;
    planner.settings.axis_steps_per_mm[X_AXIS] = util::DEFAULT_STEPS_POR_MM_X * steps_por_mm_ratio;
    planner.settings.axis_steps_per_mm[Y_AXIS] = util::DEFAULT_STEPS_POR_MM_Y * steps_por_mm_ratio;
    planner.refresh_positioning();

    if (despejar_agua)
        Bico::the().despejar_volume(duracao, volume_agua, Bico::CorrigirFluxo::Sim);

    for (auto const diametro : diametros) {
        char buffer_diametro[16] = {};
        dtostrf(diametro / steps_por_mm_ratio, 0, 2, buffer_diametro);

        char buffer_raio[16] = {};
        dtostrf((diametro / 2.f) / steps_por_mm_ratio, 0, 2, buffer_raio);

        executar_fmt("G2 F5000 X%s I%s", buffer_diametro, buffer_raio);

        posicao_final.x += diametro;
        if (associado_a_estacao and not Fila::the().executando()) {
            L0_LOG("receita foi cancelada, abortando");
            vaza = true;
            break;
        }
    }

    if (despejar_agua)
        Bico::the().desligar();

    Bico::the().aguardar_viagem_terminar();

    current_position = posicao_final;
    L0_LOG("posicao x para o marlin: ", current_position.x);

    soft_endstop._enabled = true;
    planner.settings.axis_steps_per_mm[X_AXIS] = util::DEFAULT_STEPS_POR_MM_X;
    planner.settings.axis_steps_per_mm[Y_AXIS] = util::DEFAULT_STEPS_POR_MM_Y;
    planner.refresh_positioning();

    if (vaza)
        return;

    if (series % 2 != comecar_na_borda) {
        executar_fmt("G0 F50000 X%s", buffer_raio);
        Bico::the().aguardar_viagem_terminar();
    }

    current_position = posicao_inicial;
    sync_plan_position();
}
}
