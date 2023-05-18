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
        UPDATE(LUCAS_UPDATE_ESTACAO_ATIVA, estacao.numero());
        estacao.atualizar_status("Executando");

        const auto& bico = Bico::the();
        bico.descartar_agua_ruim();
        bico.viajar_para_estacao(estacao);
#if LUCAS_DEBUG_GCODE
        LOG("-------- receita --------\n", estacao.receita().c_str() + estacao.progresso_receita());
        LOG("-------------------------");
#endif
    } else {
        UPDATE(LUCAS_UPDATE_ESTACAO_ATIVA, "-");
        gcode::parar_fila();
    }
}

void Estacao::enviar_receita(std::string receita) {
    set_receita(std::move(receita));
    set_livre(false);
    set_aguardando_input(true);
}

void Estacao::prosseguir_receita() {
    executar_instrucao(proxima_instrucao());
}

void Estacao::disponibilizar_para_uso() {
    if (m_receita.empty())
        return;

    ESTACAO_LOG("disponibilizada para uso");
    set_livre(false);
    set_aguardando_input(false);
    if (!Estacao::ativa())
        Estacao::set_estacao_ativa(this);
    else
        atualizar_status("Na fila");
}

void Estacao::cancelar_receita() {
    reset();
    set_receita_cancelada(true);
    ESTACAO_LOG("RECEITA CANCELADA!");
}

void Estacao::pausar(millis_t duracao) {
    m_pausada = true;
    m_comeco_pausa = millis();
    m_duracao_pausa = duracao;
    ESTACAO_LOG("pausando por ", duracao, "ms");
    atualizar_status("Pausada");
    procurar_nova_ativa();
}

void Estacao::despausar() {
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

void Estacao::atualizar_status(const char* str) const {
    static char nome_buffer[] = "statusE?";
    nome_buffer[7] = numero() + '0';
    UPDATE(nome_buffer, str);
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
    m_aguardando_input = b;
    if (m_aguardando_input)
        atualizar_status("Aguardando input");
}

void Estacao::set_livre(bool b) {
    m_livre = b;
    if (m_livre)
        atualizar_status("Livre");
}

std::string_view Estacao::proxima_instrucao() const {
    return gcode::proxima_instrucao(m_receita.data() + m_progresso_receita);
}

void Estacao::executar_instrucao(std::string_view instrucao) {
    atualizar_campo_gcode(CampoGcode::Atual, instrucao);
    if (gcode::ultima_instrucao(instrucao.data())) {
        atualizar_campo_gcode(CampoGcode::Proximo, "-");
        // podemos 'executar()' aqui pois, como se trata da ultima instrucao, a string é null-terminated
        gcode::executar(instrucao.data());
        ESTACAO_LOG("receita finalizada");
        reset();
    } else {
        gcode::injetar(instrucao.data());
        m_progresso_receita += instrucao.size() + 1;
        atualizar_campo_gcode(CampoGcode::Proximo, proxima_instrucao());
    }
}

void Estacao::reset() {
    set_livre(true);
    set_aguardando_input(false);
    m_pausada = false;
    m_comeco_pausa = 0;
    m_duracao_pausa = 0;
    m_progresso_receita = 0;
    m_receita.clear();

    if (esta_ativa())
        procurar_nova_ativa();

    atualizar_campo_gcode(CampoGcode::Atual, "-");
    atualizar_campo_gcode(CampoGcode::Proximo, "-");
}
}
