#include "boca.h"
#include <memory>
#include <src/module/temperature.h>

namespace kofy {

Boca::Lista Boca::s_lista = {};

static void boca_ativa_mudou() {
    static constexpr auto temp_ideal = 90;
    static auto temp_ideal_str = std::to_string(temp_ideal) + '\n';
    static constexpr auto margem_erro_temp = 10;

    static constexpr auto distancia_primeira_boca = 80;
    static constexpr auto distancia_entre_bocas = 160;

    // a segunda linha desse gcode indica a posição que será feita o descarte da água
    static constexpr auto descartar_agua_e_aguardar_temp_ideal =
R"(G0 F50000 Y60 X10
G4 P3000
G0 F1000 E100
M109 T0 R)";

    static constexpr auto mover_ate_boca_correta =
R"(G0 F50000 Y60 X)";

    static constexpr auto usar_movimento_absoluto = "G90\n";
    static constexpr auto usar_movimento_relativo = "\nG91";

    // essa variável tem que ser 'static' pois a memória dela tem que viver o suficiente para os gcodes serem executados
    static std::string rotina_troca_de_boca_ativa = "";
    // e significa que tem que ser limpa toda vez que essa função é chamada
    rotina_troca_de_boca_ativa.clear();

    // começamos a rotina indicando ao marlin que os gcodes devem ser executados em coodernadas absolutas
    rotina_troca_de_boca_ativa.append(usar_movimento_absoluto);

	#if KOFY_CUIDAR_TEMP
    // se a temperatura não é ideal (dentro da margem de erro) nós temos que regulariza-la antes de começarmos a receita
    if (abs(thermalManager.wholeDegHotend(0) - temp_ideal) >= margem_erro_temp)
       rotina_troca_de_boca_ativa.append(descartar_agua_e_aguardar_temp_ideal).append(temp_ideal_str);
	#endif

    auto posicao_absoluta_boca_ativa = distancia_primeira_boca + Boca::boca_ativa()->numero() * distancia_entre_bocas;

    // posiciona o bico na boca correta
    rotina_troca_de_boca_ativa.append(mover_ate_boca_correta).append(std::to_string(posicao_absoluta_boca_ativa));

    // volta para movimentação relativa
    rotina_troca_de_boca_ativa.append(usar_movimento_relativo);

#if KOFY_ROTINA_TROCA
    gcode::injetar(rotina_troca_de_boca_ativa);

    DBG("executando rotina da troca de bocas");

    #if KOFY_DEBUG_GCODE
    DBG("---- gcode -----\n", rotina_troca_de_boca_ativa.c_str());
    #endif

    g_mudando_boca_ativa = true;

#endif
}

// TODO: desenvolver um algoritmo legal pra isso...
void Boca::procurar_nova_boca_ativa() {
    DBG("procurando nova boca ativa...");

    for (auto& boca : s_lista) {
        if (!boca.aguardando_botao()) {
            DBG("boca #", boca.numero(), " foi selecionada!");
            s_boca_ativa = &boca;
            boca_ativa_mudou();
            return;
        }
    }

    DBG("nenhuma boca está disponível. :(");

    s_boca_ativa = nullptr;
}

void Boca::set_boca_ativa(Boca* boca) {
    if (boca == s_boca_ativa)
        return;

    s_boca_ativa = boca;

    if (boca) {
        DBG("boca #", boca->numero(), " foi manualmente selecionada.");
        boca_ativa_mudou();
    }
}

std::string_view Boca::proxima_instrucao() const {
	return gcode::proxima_instrucao(m_receita.data() + m_progresso_receita);
}

void Boca::prosseguir_receita() {
	auto instrucao_atual = m_receita.data() + m_progresso_receita;
	gcode::injetar(instrucao_atual);
	if (queue.injected_commands_P) {
    	m_progresso_receita += queue.injected_commands_P - instrucao_atual;
	} else {
		m_progresso_receita = 0;
		aguardar_botao();
	}
}

void Boca::disponibilizar_para_uso() {
    DBG("disponibilizando boca #", numero(), " para uso.");
    m_aguardando_botao = false;
}

size_t Boca::numero() const {
    return ((uintptr_t)this - (uintptr_t)&s_lista) / sizeof(Boca);
}

void Boca::finalizar_receita() {
    DBG("finalizando a receita da boca #", numero(), ".");

    executar_instrucao(proxima_instrucao());
    reiniciar_progresso();
    aguardar_botao();
}

void Boca::executar_instrucao(std::string_view instrucao) {
    #if KOFY_DEBUG_GCODE
        std::string str(instrucao);
        DBG("executando gcode: ", str.data());
    #endif
    gcode::injetar(instrucao);
    m_progresso_receita += queue.injected_commands_P - (m_receita.data() + m_progresso_receita);
}

}
