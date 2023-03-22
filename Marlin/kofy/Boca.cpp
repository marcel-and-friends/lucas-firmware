#include "boca.h"
#include <memory>
#include <src/module/temperature.h>

namespace kofy {

Boca::Lista Boca::s_lista = {};

static void boca_ativa_mudou() {

    static constexpr auto TEMP_IDEAL = 90;
    static constexpr auto MARGEM_ERRO = 10;

    static constexpr auto DISTANCIA_ENTRE_BOCAS = 160;
    static constexpr auto DISTANCIA_PRIMEIRA_BOCA = 80;

    static constexpr auto RETORNA_HOME_E_ESPERA_TEMP = 
R"(M302 S0
G90
G0 F50000 Y60 X720
G4 P3000
G0 F1000 E100
M109 T0 R)";

    static constexpr auto MOVE_ATE_BOCA_CORRETA = 
R"(M302 S0
G90
G0 F50000 Y60 X)";

    static constexpr auto ATIVA_POSICAO_RELATIVA = "\nG91";

    // essa variável tem que ser 'static' pois a memória dela tem que viver o suficiente pros gcodes serem executados
    static std::string gcode = "";
    // e significa que tem que ser limpa toda vez que essa função é chamada
    gcode.clear();

    if (abs(thermalManager.wholeDegHotend(0) - TEMP_IDEAL) >= MARGEM_ERRO) {
        gcode.append(RETORNA_HOME_E_ESPERA_TEMP).append(std::to_string(TEMP_IDEAL)).append("\n");
    }

    auto posicao_boca = DISTANCIA_PRIMEIRA_BOCA + Boca::boca_ativa()->numero() * DISTANCIA_ENTRE_BOCAS;

    gcode.append(MOVE_ATE_BOCA_CORRETA)
    .append(std::to_string(posicao_boca))
    .append(ATIVA_POSICAO_RELATIVA);

    marlin::injetar_gcode(gcode);

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
    procurar_nova_boca_ativa();

    marlin::parar_fila_de_gcode();
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