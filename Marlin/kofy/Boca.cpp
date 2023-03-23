#include "boca.h"
#include <memory>
#include <src/module/temperature.h>

namespace kofy {

Boca::Lista Boca::s_lista = {};

static void boca_ativa_mudou() {
    static constexpr auto temp_ideal = 90;
    static auto temp_ideal_str = std::to_string(temp_ideal) + '\n';
    static constexpr auto margem_erro_temp = 10;

    static constexpr auto distancia_entre_bocas = 160;
    static constexpr auto distancia_primeira_boca = 80;

    // a segunda linha desse gcode indica a posição que será feita o descarte da água
    static constexpr auto descartar_agua_e_aguardar_temp_ideal = 
R"(M302 S0
G0 F50000 Y60 X10
G4 P3000
G0 F1000 E100
M109 T0 R)";

    static constexpr auto mover_ate_boca_correta = 
R"(M302 S0
G0 F50000 Y60 X)";

    static constexpr auto usar_movimento_absoluto = "G90\n";
    static constexpr auto usar_movimento_relativo = "\nG91";

    // essa variável tem que ser 'static' pois a memória dela tem que viver o suficiente para os gcodes serem executados
    static std::string rotina_troca_de_boca_ativa = "";
    // e significa que tem que ser limpa toda vez que essa função é chamada
    rotina_troca_de_boca_ativa.clear();

    // começamos a rotina indicando ao marlin que os gcodes devem ser executados em coodernadas absolutas
    rotina_troca_de_boca_ativa.append(usar_movimento_absoluto);

    // se a temperatura não é ideal (dentro da margem de erro) nós temos que regulariza-la antes de começarmos a receita
    if (abs(thermalManager.wholeDegHotend(0) - temp_ideal) >= margem_erro_temp) {
       rotina_troca_de_boca_ativa.append(descartar_agua_e_aguardar_temp_ideal).append(temp_ideal_str);
    }

    auto posicao_absoluta_boca_ativa = distancia_primeira_boca + Boca::boca_ativa()->numero() * distancia_entre_bocas;

    // posiciona o bico na boca correta
    rotina_troca_de_boca_ativa.append(mover_ate_boca_correta).append(std::to_string(posicao_absoluta_boca_ativa));
    
    // volta para movimentação relativa
    rotina_troca_de_boca_ativa.append(usar_movimento_relativo);

    marlin::injetar_gcode(rotina_troca_de_boca_ativa);

    DBG("executando rotina da troca de bocas");

    #if DEBUG_GCODES
    DBG("---- gcode -----\n", rotina_troca_de_boca_ativa.c_str());
    #endif

    g_mudando_boca_ativa = true;
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
    auto proxima_instrucao = m_receita.data() + m_progresso_receita;
    // como uma sequência de gcodes é separada pelo caractere de nova linha basta procurar um desse para encontrar o fim
    auto fim_do_proximo_gcode = strchr(proxima_instrucao, '\n');
    if (!fim_do_proximo_gcode) {
        // se a nova linha não existe então estamos no último
        return proxima_instrucao;
    }

    return std::string_view{ proxima_instrucao, static_cast<size_t>(fim_do_proximo_gcode - proxima_instrucao)};
}

void Boca::progredir_receita() {
    auto proxima_instrucao = this->proxima_instrucao();
    // se não há uma nova linha após a próxima instrução então ela é a última
    bool ultima_instrucao = !strchr(proxima_instrucao.data(), '\n');
    ultima_instrucao ? finalizar_receita() : executar_instrucao(proxima_instrucao);
}

void Boca::pular_proxima_instrucao() {
    m_progresso_receita += proxima_instrucao().size() + 1;
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
    reiniciar_receita();
    aguardar_botao();
    marlin::parar_fila_de_gcode();
    procurar_nova_boca_ativa();
}

void Boca::executar_instrucao(std::string_view instrucao) {
    #if DEBUG_GCODES
        std::string str(instrucao);
        DBG("executando gcode: ", str.data());
    #endif
    marlin::injetar_gcode(instrucao);
    m_progresso_receita += instrucao.size() + 1;
}

}