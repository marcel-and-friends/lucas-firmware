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
        millis_t comeco_abs = 0;

        // a duracao total
        millis_t duracao = 0;

        // o tempo de intervalo entre esse e o proximo passo
        millis_t intervalo = 0;

        // o progresso desse passo, enviado para o app
        millis_t progresso = 0;

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

    size_t passo_atual_index() const;

    bool acabou() const;

    const Passo& primeiro_passo() const;

    const Passo& passo_num(size_t) const;

public:
    void for_each_passo(util::IterFn<Passo&> auto&& callback) {
        if (m_escaldo.has_value())
            if (std::invoke(FWD(callback), m_escaldo.value()) == util::Iter::Break)
                return;

        for (size_t i = 0; i < m_num_ataques; ++i)
            if (std::invoke(FWD(callback), m_ataques[i]) == util::Iter::Break)
                return;
    }

    void for_each_passo_pendente(util::IterFn<Passo&> auto&& callback) {
        if (m_escaldo.has_value() && !m_escaldou) {
            std::invoke(FWD(callback), m_escaldo.value());
            return;
        }

        for (size_t i = m_ataque_atual; i < m_num_ataques; ++i)
            if (std::invoke(FWD(callback), m_ataques[i]) == util::Iter::Break)
                return;
    }

    void for_each_passo_pendente(util::IterFn<Passo&, size_t> auto&& callback) {
        if (m_escaldo.has_value() && !m_escaldou) {
            std::invoke(FWD(callback), m_escaldo.value(), 0);
            return;
        }

        for (size_t i = m_ataque_atual; i < m_num_ataques; ++i)
            if (std::invoke(FWD(callback), m_ataques[i], i + 1) == util::Iter::Break)
                return;
    }

    void for_each_passo_pendente(util::IterFn<const Passo&> auto&& callback) const {
        if (m_escaldo.has_value() && !m_escaldou) {
            std::invoke(FWD(callback), m_escaldo.value());
            return;
        }

        for (size_t i = m_ataque_atual; i < m_num_ataques; ++i)
            if (std::invoke(FWD(callback), m_ataques[i]) == util::Iter::Break)
                return;
    }

    void for_each_passo_pendente(util::IterFn<const Passo&, size_t> auto&& callback) const {
        if (m_escaldo.has_value() && !m_escaldou) {
            std::invoke(FWD(callback), m_escaldo.value(), 0);
            return;
        }

        for (size_t i = m_ataque_atual; i < m_num_ataques; ++i)
            if (std::invoke(FWD(callback), m_ataques[i], i + 1) == util::Iter::Break)
                return;
    }

public:
    bool possui_escaldo() const { return m_escaldo.has_value(); }
    const Passo& escaldo() const { return m_escaldo.value(); }
    Passo& escaldo() { return m_escaldo.value(); }

    millis_t tempo_de_finalizacao() const { return m_tempo_de_finalizacao; }

    millis_t inicio_tempo_de_finalizacao() const { return m_inicio_tempo_finalizacao; }
    void set_inicio_tempo_finalizacao(millis_t tick) { m_inicio_tempo_finalizacao = tick; }

    uint32_t id() const { return m_id; }

    bool escaldou() const { return m_escaldou; }

    std::span<const Passo> ataques() const { return { m_ataques.begin(), m_num_ataques }; }
    std::span<Passo> ataques() { return { m_ataques.begin(), m_num_ataques }; }

    const Passo& passo_atual() const;
    Passo& passo_atual();

private:
    void resetar();

    friend class Fila;
    Receita() = default;

    uint32_t m_id = 0;

    std::optional<Passo> m_escaldo = std::nullopt;
    bool m_escaldou = false;

    millis_t m_tempo_de_finalizacao = 0;
    millis_t m_inicio_tempo_finalizacao = 0;

    static constexpr auto MAX_ATAQUES = 10; // foi decidido por marcel

    // finge que isso é um vector
    size_t m_num_ataques = 0;
    std::array<Passo, MAX_ATAQUES> m_ataques = {};
    size_t m_ataque_atual = 0;
};
}
