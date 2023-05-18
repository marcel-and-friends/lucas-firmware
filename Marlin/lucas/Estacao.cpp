#include "Estacao.h"
#include <memory>
#include <src/module/temperature.h>
#include <lucas/Bico.h>

#define ESTACAO_LOG(...) LOG("estacao #", this->numero(), " - ", __VA_ARGS__)

namespace lucas {
Estacao::Lista Estacao::s_lista = {};

// TODO: desenvolver um algoritmo legal pra isso...
void Estacao::procurar_nova_ativa() {
    LOG("procurando nova estacao ativa...");

    for (auto& estacao : s_lista)
        if (estacao.disponivel_para_uso())
            return set_estacao_ativa(&estacao);

    LOG("nenhuma estacao esta disponivel :(");
    set_estacao_ativa(nullptr);
}

void Estacao::set_estacao_ativa(Estacao* estacao) {
    if (estacao == s_estacao_ativa)
        return;

    s_estacao_ativa = estacao;
    if (s_estacao_ativa) {
        auto& estacao = *s_estacao_ativa;
        LOG("estacao #", estacao.numero(), " - escolhida como nova estacao ativa");

        const auto& bico = Bico::the();
        bico.descartar_agua_ruim();
        bico.viajar_para_estacao(estacao);

#if LUCAS_DEBUG_GCODE
        auto gcode = estacao.receita().c_str() + estacao.progresso_receita();
        LOG("-------- receita --------\n", gcode);
        LOG("-------------------------");
#endif
        Estacao::ativa()->atualizar_status("Executando");
        UPDATE(LUCAS_UPDATE_ESTACAO_ATIVA, Estacao::ativa()->numero());
    } else {
        UPDATE(LUCAS_UPDATE_ESTACAO_ATIVA, "-");
        gcode::parar_fila();
    }
}

void Estacao::ativa_prestes_a_comecar() {
}

void Estacao::prosseguir_receita() {
    executar_instrucao(proxima_instrucao());
}

void Estacao::preparar_para_finalizar_receita() {
    set_livre(true);
    set_aguardando_input(false);
    m_receita.clear();
    m_progresso_receita = 0;
    m_pausada = false;
    m_comeco_pausa = 0;
    m_duracao_pausa = 0;
}

void Estacao::disponibilizar_para_uso() {
    ESTACAO_LOG("disponibilizada para uso");
    set_livre(false);
    set_aguardando_input(false);
    m_pausada = false;
    if (!Estacao::ativa())
        Estacao::set_estacao_ativa(this);
    else
        atualizar_status("Na fila");
}

void Estacao::aguardar_input() {
    m_aguardando_input = true;
    atualizar_status("Aguardando input");
}

void Estacao::cancelar_receita() {
    set_cancelada(true);
    preparar_para_finalizar_receita();
    if (esta_ativa())
        set_estacao_ativa(nullptr);

    atualizar_status("Livre");
    atualizar_campo_gcode(CampoGcode::Atual, "-");
    atualizar_campo_gcode(CampoGcode::Proximo, "-");
    ESTACAO_LOG("RECEITA CANCELADA!");
}

void Estacao::receita_finalizada() {
    atualizar_status("Livre");
    // o campo 'Proximo' já foi atualizado dentro da função 'prosseguir_receita'
    atualizar_campo_gcode(CampoGcode::Atual, "-");
    procurar_nova_ativa();
}

void Estacao::pausar(millis_t duracao) {
    m_pausada = true;
    m_comeco_pausa = millis();
    m_duracao_pausa = duracao;
    ESTACAO_LOG("pausando por ", duracao, "ms");
    atualizar_status("Pausada");
}

bool Estacao::tempo_de_pausa_atingido(millis_t tick) const {
    return tick - m_comeco_pausa >= m_duracao_pausa;
}

bool Estacao::disponivel_para_uso() const {
    return !esta_ativa() && !livre() && !aguardando_input() && !pausada();
}

bool Estacao::esta_ativa() const {
    return Estacao::ativa() == this;
}

void Estacao::atualizar_campo_gcode(CampoGcode qual, std::string_view valor) const {
    static char valor_buffer[64] = {};
    static char nome_buffer[] = "gCode?E?";

    nome_buffer[5] = static_cast<char>(qual) + '0';
    nome_buffer[7] = numero() + '0';

    memcpy(valor_buffer, valor.data(), valor.size());
    valor_buffer[valor.size()] = '\0';
    UPDATE(nome_buffer, valor_buffer);
}

void Estacao::atualizar_status(std::string_view str) const {
    static char nome_buffer[] = "statusE?";
    nome_buffer[7] = numero() + '0';
    UPDATE(nome_buffer, str.data());
}

float Estacao::posicao_absoluta() const {
    static constexpr auto distancia_primeira_estacao = 80.f;
    static constexpr auto distancia_entre_estacoes = 160.f;
    return distancia_primeira_estacao + index() * distancia_entre_estacoes;
}

size_t Estacao::numero() const {
    return index() + 1;
}

size_t Estacao::index() const {
    return ((uintptr_t)this - (uintptr_t)&s_lista) / sizeof(Estacao);
}

void Estacao::set_aguardando_input(bool b) {
    if (b) {
        aguardar_input();
    } else {
        m_aguardando_input = false;
    }
}

void Estacao::executar_rotina_troca_de_estacao_ativa() {
}

std::string_view Estacao::proxima_instrucao() const {
    return gcode::proxima_instrucao(m_receita.data() + m_progresso_receita);
}

void Estacao::executar_instrucao(std::string_view instrucao) {
    gcode::injetar(instrucao.data());
    atualizar_campo_gcode(CampoGcode::Atual, instrucao);
    if (gcode::ultima_instrucao(instrucao.data())) {
        atualizar_campo_gcode(CampoGcode::Proximo, "-");
        preparar_para_finalizar_receita();
    } else {
        m_progresso_receita += instrucao.size() + 1;
        atualizar_campo_gcode(CampoGcode::Proximo, proxima_instrucao());
    }
}
}
