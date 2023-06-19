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
        // o offset do primeiro ataque onde a receita começa
        millis_t comeco_rel = 0;

        millis_t comeco_abs = 0;

        // a duracao total
        millis_t duracao = 0;

        // o tempo de intervalo entre esse e o proximo passo
        millis_t intervalo = 0;

        // o progresso desse passo, enviado para o app
        millis_t progresso = 0;

        // o gcode desse passo
        char gcode[64] = {};

        millis_t fim() const {
            if (!comeco_abs)
                return 0;
            return comeco_abs + duracao;
        }

        bool colide_com(const Passo& outro) const {
            // dois ataques colidem se:

            // 1. um comeca dentro do outro
            if (this->comeco_abs >= outro.comeco_abs && this->comeco_abs <= outro.fim())
                return true;

            // 2. um termina dentro do outro
            if (this->fim() >= outro.comeco_abs && this->fim() <= outro.fim())
                return true;

            // 3. a distancia entre o comeco de um e o fim do outro é menor que o tempo de viagem de uma estacao à outra
            if (this->comeco_abs > outro.fim() && this->comeco_abs - outro.fim() < util::MARGEM_DE_VIAGEM)
                return true;

            // 4. a distancia entre o fim de um e o comeco do outro é menor que o tempo de viagem de uma estacao à outra
            if (this->fim() < outro.comeco_abs && outro.comeco_abs - this->fim() < util::MARGEM_DE_VIAGEM)
                return true;

            return false;
        }
    };

public:
    static std::unique_ptr<Receita> padrao();

    static std::unique_ptr<Receita> from_json(JsonObjectConst json);

    Receita() = default;

    Receita(const Receita&) = delete;
    Receita& operator=(const Receita&) = delete;
    Receita(Receita&&) = delete;
    Receita& operator=(Receita&&) = delete;

public:
    enum class Acabou {
        Sim = 0,
        Nao
    };
    Acabou executar_passo();

    void mapear_passos_pendentes(millis_t tick_inicial);

    bool passos_pendentes_estao_mapeados();

public:
    void for_each_passo_pendente(util::IterCallback<Passo&> auto&& callback) {
        if (m_escaldo.has_value() && !m_escaldou) {
            std::invoke(FWD(callback), m_escaldo.value());
            return;
        }

        for_each_ataque_pendente(FWD(callback));
    }

    void for_each_passo_pendente(util::IterCallback<Passo&, size_t> auto&& callback) {
        if (m_escaldo.has_value() && !m_escaldou) {
            std::invoke(FWD(callback), m_escaldo.value(), 0);
            return;
        }

        for_each_ataque_pendente(FWD(callback));
    }

    void for_each_ataque_pendente(util::IterCallback<Passo&> auto&& callback) {
        for (size_t i = m_ataque_atual; i < m_num_ataques; ++i) {
            if (std::invoke(FWD(callback), m_ataques[i]) == util::Iter::Stop)
                break;
        }
    }

    void for_each_ataque_pendente(util::IterCallback<Passo&, size_t> auto&& callback) {
        for (size_t i = m_ataque_atual; i < m_num_ataques; ++i) {
            if (std::invoke(FWD(callback), m_ataques[i], i) == util::Iter::Stop)
                break;
        }
    }

public:
    bool possui_escaldo() const { return m_escaldo.has_value(); }
    const Passo& escaldo() const { return m_escaldo.value(); }
    Passo& escaldo() { return m_escaldo.value(); }

    size_t id() const { return m_id; }

    bool escaldou() const { return m_escaldou; }

    std::span<const Passo> ataques() const { return { m_ataques.begin(), m_num_ataques }; }
    std::span<Passo> ataques() { return { m_ataques.begin(), m_num_ataques }; }

    const Passo& passo_atual() const;
    Passo& passo_atual();

    size_t passo_atual_idx() const;

private:
    // id da receita, usado pelo app
    size_t m_id = 0;

    // o passo do escaldo, caso a receita possua um
    std::optional<Passo> m_escaldo;

    bool m_escaldou = false;

    // o tempo total que essa receita consumirá
    millis_t m_tempo_total = 0;

    // o tempo de finalização dessa receita
    millis_t m_finalizacao = 0;

    static constexpr auto MAX_ATAQUES = 10; // foi decidido por marcel

    // lista de todos os nossos ataques
    size_t m_num_ataques = 0;
    std::array<Passo, MAX_ATAQUES> m_ataques = {};

    // o ataque atual que estamos da receita
    size_t m_ataque_atual = 0;
};
}
