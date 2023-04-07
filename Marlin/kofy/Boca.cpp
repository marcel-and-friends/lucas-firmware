#include "boca.h"
#include <memory>
#include <src/module/temperature.h>

namespace kofy {

Boca::Lista Boca::s_lista = {};

namespace util {
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

    static constexpr auto mover_ate_boca_correta = "G0 F50000 Y60 X";

    static constexpr auto usar_movimento_absoluto = "G90\n";
    static constexpr auto usar_movimento_relativo = "\nG91";
}

static void boca_ativa_mudou() {
	#if KOFY_DEBUG_GCODE
		auto gcode = Boca::ativa()->receita().c_str() + Boca::ativa()->progresso_receita();
		DBG("-------- receita --------\n", gcode);
		DBG("-------------------------");
	#endif
    // essa variável tem que ser 'static' pois a memória dela tem que viver o suficiente para os gcodes serem executados
    static std::string rotina_troca_de_boca_ativa = "";
    // e significa que tem que ser limpa toda vez que essa função é chamada
    rotina_troca_de_boca_ativa.clear();
    // começamos a rotina indicando ao marlin que os gcodes devem ser executados em coodernadas absolutas
    rotina_troca_de_boca_ativa.append(util::usar_movimento_absoluto);

	#if KOFY_CUIDAR_TEMP
    	// se a temperatura não é ideal (dentro da margem de erro) nós temos que regulariza-la antes de começarmos a receita
    	if (abs(thermalManager.wholeDegHotend(0) - util::temp_ideal) >= util::margem_erro_temp)
    	   rotina_troca_de_boca_ativa.append(util::descartar_agua_e_aguardar_temp_ideal).append(util::temp_ideal_str);
	#endif

    auto posicao_absoluta_boca_ativa = util::distancia_primeira_boca + Boca::ativa()->numero() * util::distancia_entre_bocas;
    // posiciona o bico na boca correta
    rotina_troca_de_boca_ativa.append(util::mover_ate_boca_correta).append(std::to_string(posicao_absoluta_boca_ativa));
    // volta para movimentação relativa
    rotina_troca_de_boca_ativa.append(util::usar_movimento_relativo);

	#if KOFY_ROTINA_TROCA
    	DBG("executando rotina da troca de boca ativa");
    	gcode::injetar(rotina_troca_de_boca_ativa);
    	#if KOFY_DEBUG_GCODE
    		DBG("---- gcode da rotina ----\n", rotina_troca_de_boca_ativa.c_str());
			DBG("-------------------------");
    	#endif
    	g_mudando_boca_ativa = true;
	#endif
}

// TODO: desenvolver um algoritmo legal pra isso...
void Boca::procurar_nova_boca_ativa() {
    DBG("procurando nova boca ativa...");

    for (auto& boca : s_lista) {
        if (!boca.aguardando_botao()) {
            DBG("boca #", boca.numero(), " - escolhida como nova boca ativa");
            s_boca_ativa = &boca;
            boca_ativa_mudou();
            return;
        }
    }

    DBG("nenhuma boca está disponível :(");

    s_boca_ativa = nullptr;
}

void Boca::set_boca_ativa(Boca* boca) {
    if (boca == s_boca_ativa)
        return;

    s_boca_ativa = boca;

    if (s_boca_ativa) {
        DBG("boca #", boca->numero(), " foi manualmente selecionada para ser a nova boca ativa");
        boca_ativa_mudou();
    }
}

std::string_view Boca::proxima_instrucao() const {
	return gcode::proxima_instrucao(m_receita.data() + m_progresso_receita);
}

void Boca::prosseguir_receita() {
	auto proxima_instrucao = this->proxima_instrucao();
	executar_instrucao(proxima_instrucao);
}

void Boca::disponibilizar_para_uso() {
    DBG("boca #", numero(), " - disponibilizada para uso");
    m_aguardando_botao = false;
}

size_t Boca::numero() const {
    return ((uintptr_t)this - (uintptr_t)&s_lista) / sizeof(Boca);
}

void Boca::cancelar_receita() {
	reiniciar_receita();
	set_botao_apertado(false);
	aguardar_botao();
	if (Boca::ativa() == this) {
		Boca::set_boca_ativa(nullptr);
		gcode::parar_fila();
	}
}

void Boca::executar_instrucao(std::string_view instrucao) {
    #if KOFY_DEBUG_GCODE
		static char buf[256] = {};
		memcpy(buf, instrucao.data(), instrucao.size());
		buf[instrucao.size()] = '\0';
        DBG("boca #", numero(), " - executando instrução '", buf, "'");
    #endif
    gcode::injetar(instrucao);
	if (!gcode::e_ultima_instrucao(instrucao)) {
		m_progresso_receita += instrucao.size() + 1;
	} else {
		// o proximo gcode é o último...
		reiniciar_progresso();
		aguardar_botao();
	}
}

}
