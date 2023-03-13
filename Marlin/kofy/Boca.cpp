#include "boca.h"
#include <memory>

namespace kofy {

Boca::Lista Boca::s_lista = {};

static void boca_ativa_mudou() {
    static constexpr auto parte_1 = 
R"(M302 S0
G90
G0 F50000 Y60 X)";
    static constexpr auto parte_2 = "\nG92";

    auto posicao_x = 80 + Boca::boca_ativa()->numero() * 160;

    g_gcode.clear();
    g_gcode.append(parte_1);
    g_gcode.append(std::to_string(posicao_x));
    g_gcode.append(parte_2);

    marlin::injetar_gcode(g_gcode);

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