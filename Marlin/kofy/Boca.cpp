#include "boca.h"

namespace kofy {

Boca::Lista Boca::s_lista = {};

// TODO: desenvolver um algoritmo legal pra isso...
void Boca::procurar_nova_boca_ativa() {
    DBG("procurando nova boca ativa...");

    for (auto& boca : s_lista) {
        if (!boca.aguardando_botao()) {
            DBG("boca #", boca.numero(), " foi selecionada!");
            s_boca_ativa = &boca;
            return;
        }
    }

    DBG("nenhuma boca está disponível. :(");
    s_boca_ativa = nullptr;
}

void Boca::set_boca_ativa(Boca* boca) {
    if (boca)
        DBG("boca #", boca->numero(), " foi manualmente selecionada.");
    
    s_boca_ativa = boca;
}

std::string_view Boca::proxima_instrucao() const {
    auto proxima_instrucao = std::string_view{ m_receita.data() + m_progresso_receita };
    // como uma sequência de gcodes é separada pelo caractere de nova linha basta procurar um desse para encontrar o fim
    auto fim_do_proximo_gcode = proxima_instrucao.find('\n');
    if (fim_do_proximo_gcode == std::string_view::npos) {
        // se a nova linha não existe então estamos no último
        return proxima_instrucao;
    }

    return std::string_view{ proxima_instrucao.data(), fim_do_proximo_gcode };
}

void Boca::progredir_receita() {
    auto proxima_instrucao = this->proxima_instrucao();
    // se não há uma nova linha após a próxima instrução então ela é a última
    bool ultima_instrucao = !proxima_instrucao.find('\n'); 
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
    marlin::injetar_gcode(instrucao);
    m_progresso_receita += instrucao.size() + 1;
}

}