#include "Fila.h"
#include <lucas/Bico.h>

namespace lucas {
constexpr auto TEMPO_MAX_VIAGEM = 2000;

#define FILA_LOG(...) LOG("", "fila - ", __VA_ARGS__);

bool Fila::executando() const {
    return m_receita_ativa;
}

void Fila::agendar_receita(Estacao::Index estacao_idx, std::unique_ptr<Receita> receita_ptr) {
    auto& estacao = Estacao::lista().at(estacao_idx);
    if (m_receitas.empty()) {
        auto& receita = *receita_ptr;
        m_receitas.emplace(estacao_idx, std::move(receita_ptr));
        Bico::the().viajar_para_estacao(estacao);
        if (receita.possui_escaldo()) {
            FILA_LOG("receita possui escaldo");
            estacao.set_status(Estacao::Status::SCALDING);

            m_receita_ativa = &receita;
            receita.executar_escaldo();
            m_receita_ativa = nullptr;

            estacao.set_status(Estacao::Status::INITIALIZE_COFFEE);
            estacao.set_aguardando_input(true);
            FILA_LOG("escaldo finalizado");
        }
        mapear_receita(estacao_idx);
    } else {
        FILA_LOG("mucha cosa >.<");
    }
}

void Fila::tick(millis_t tick) {
    if (m_receita_ativa || m_receitas.empty())
        return;

    for_each_receita([this, tick](Estacao::Index estacao_idx, Receita& receita) {
        if (receita.ataque_atual().comeco_abs == tick) {
            FILA_LOG("ataque bateu! [tick = ", tick, "]");

            m_receita_ativa = &receita;
            receita.executar_ataque();
            if (receita.terminou()) {
                m_receitas.erase(estacao_idx);
                FILA_LOG("receita acabou! removida do mapa");
            }
            m_receita_ativa = nullptr;
            return util::Iter::Stop;
        } else if (receita.ataque_atual().comeco_abs - tick == TEMPO_MAX_VIAGEM) {
            Bico::the().viajar_para_estacao(estacao_idx + 1);
            return util::Iter::Stop;
        }
        return util::Iter::Continue;
    });
}

void Fila::mapear_receita(Estacao::Index estacao_idx) {
    auto it = m_receitas.find(estacao_idx);
    if (it == m_receitas.end()) {
        FILA_LOG("receita nao encontrada para estacao #", estacao_idx);
        return;
    }

    auto& receita = it->second;
    auto tick_acumulado = millis() + 5;
    receita->for_each_ataque([&tick_acumulado](Receita::Passo& ataque) {
        ataque.comeco_abs = tick_acumulado;
        tick_acumulado += ataque.duracao + ataque.intervalo + TEMPO_MAX_VIAGEM * 2;
        FILA_LOG("ataque.comeco_abs = ", ataque.comeco_abs, " | tick_acumulado = ", tick_acumulado, " [duracao = ", ataque.duracao, " | intervalo = ", ataque.intervalo, "]");
        return util::Iter::Continue;
    });
}

void Fila::cancelar_receita(Estacao::Index) {
    // come back to this
}
}
