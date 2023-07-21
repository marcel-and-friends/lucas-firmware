#pragma once

#include <cstddef>
#include <concepts>
#include <cstdint>
#include <string_view>
#include <array>
#include <memory>
#include <span>
#include <src/MarlinCore.h>
#include <lucas/util/util.h>
#include <ArduinoJson.h>

namespace lucas {
class Receita {
public:
    struct Passo {
        // o tick em que esse passo será executado
        // @Fila::mapear_receita
        millis_t comeco_abs = 0;

        // a duracao total
        millis_t duracao = 0;

        // o tempo de intervalo entre esse e o proximo passo
        millis_t intervalo = 0;

        // o gcode desse passo
        char gcode[48] = {};

        millis_t fim() const {
            if (!comeco_abs)
                return 0;
            return comeco_abs + duracao;
        }

        bool colide_com(const Passo& outro) const;
    };

public:
    static JsonObjectConst padrao();

    void montar_com_json(JsonObjectConst);

    Receita(const Receita&) = delete;
    Receita(Receita&&) = delete;
    Receita& operator=(const Receita&) = delete;
    Receita& operator=(Receita&&) = delete;

public:
    void executar_passo_atual();

    void mapear_passos_pendentes(millis_t tick_inicial);

    void desmapear_passos();

    bool passos_pendentes_estao_mapeados() const;

public:
    void for_each_passo(util::IterFn<Passo&> auto&& callback) {
        for (size_t i = 0; i < m_num_passos; ++i)
            if (std::invoke(FWD(callback), m_passos[i]) == util::Iter::Break)
                return;
    }

    void for_each_passo_pendente(util::IterFn<Passo&> auto&& callback) {
        if (m_possui_escaldo && !escaldou()) {
            std::invoke(FWD(callback), m_passos.front());
            return;
        }

        for (size_t i = m_passo_atual; i < m_num_passos; ++i)
            if (std::invoke(FWD(callback), m_passos[i]) == util::Iter::Break)
                return;
    }

    void for_each_passo_pendente(util::IterFn<Passo&, size_t> auto&& callback) {
        if (m_possui_escaldo && !escaldou()) {
            std::invoke(FWD(callback), m_passos.front());
            return;
        }

        for (size_t i = m_passo_atual; i < m_num_passos; ++i)
            if (std::invoke(FWD(callback), m_passos[i], i) == util::Iter::Break)
                return;
    }

    void for_each_passo_pendente(util::IterFn<const Passo&> auto&& callback) const {
        if (m_possui_escaldo && !escaldou()) {
            std::invoke(FWD(callback), m_passos.front());
            return;
        }

        for (size_t i = m_passo_atual; i < m_num_passos; ++i)
            if (std::invoke(FWD(callback), m_passos[i]) == util::Iter::Break)
                return;
    }

    void for_each_passo_pendente(util::IterFn<const Passo&, size_t> auto&& callback) const {
        if (m_possui_escaldo && !escaldou()) {
            std::invoke(FWD(callback), m_passos.front());
            return;
        }

        for (size_t i = m_passo_atual; i < m_num_passos; ++i)
            if (std::invoke(FWD(callback), m_passos[i], i) == util::Iter::Break)
                return;
    }

public:
    bool possui_escaldo() const { return m_possui_escaldo; }
    const Passo& escaldo() const { return m_passos.front(); }
    Passo& escaldo() { return m_passos.front(); }

    millis_t tempo_de_finalizacao() const { return m_tempo_de_finalizacao; }

    millis_t inicio_tempo_de_finalizacao() const { return m_inicio_tempo_finalizacao; }
    void set_inicio_tempo_finalizacao(millis_t tick) { m_inicio_tempo_finalizacao = tick; }

    uint32_t id() const { return m_id; }

    bool escaldou() const { return m_possui_escaldo && m_passo_atual > 0; }

    std::span<const Passo> ataques() const { return { m_passos.begin() + m_possui_escaldo, m_num_passos - m_possui_escaldo }; }
    std::span<Passo> ataques() { return { m_passos.begin() + m_possui_escaldo, m_num_passos - m_possui_escaldo }; }

    const Passo& primeiro_ataque() const { return m_passos[m_possui_escaldo]; }

    const Passo& passo_atual() const { return m_passos[m_passo_atual]; }
    Passo& passo_atual() { return m_passos[m_passo_atual]; }

    size_t passo_atual_index() const { return m_passo_atual; }

    bool acabou() const { return m_passo_atual == m_num_passos; }

    const Passo& primeiro_passo() const { return m_passos.front(); }

    const Passo& passo(size_t index) const { return m_passos[index]; }

private:
    void resetar();

    // as receitas, similarmente às estacões, existem exclusivemente como elementos de um array estático, na classe Fila
    // por isso os copy/move constructors são deletados e só a Fila pode acessar o constructor default
    friend class Fila;
    Receita() = default;

    uint32_t m_id = 0;

    millis_t m_tempo_de_finalizacao = 0;
    millis_t m_inicio_tempo_finalizacao = 0;

    static constexpr auto MAX_ATAQUES = 10;             // foi decidido por marcel
    static constexpr auto MAX_PASSOS = MAX_ATAQUES + 1; // incluindo o escaldo

    bool m_possui_escaldo = false;

    // finge que isso é um vector
    size_t m_num_passos = 0;
    std::array<Passo, MAX_PASSOS> m_passos = {};

    // a index dos passos de uma receita depende da existencia do escaldo
    // o primeiro ataque de uma receita com escaldo tem index 1
    // o primeiro ataque de uma receita sem escaldo tem index 0
    size_t m_passo_atual = 0;
};
}
