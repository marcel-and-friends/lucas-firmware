#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <array>
#include <memory>
#include <src/MarlinCore.h>
#include <lucas/util/util.h>
#include <ArduinoJson.h>

namespace lucas {
class Receita {
public:
    static std::unique_ptr<Receita> padrao();

    static std::unique_ptr<Receita> from_json(JsonObjectConst json);

    Receita() = default;

    bool terminou() const;

    void prosseguir();

    size_t id() const {
        return m_id;
    }

    void for_each_ataque(auto&& fn) {
        for (size_t i = 0; i < m_num_ataques; ++i) {
            if (std::invoke(fn, m_ataques[i]) == util::Iter::Stop)
                break;
        }
    }

    struct Passo {
        // o offset do primeiro ataque onde a receita começa
        millis_t comeco = 0;

        // a duracao total
        millis_t duracao = 0;

        // o tempo de intervalo entre esse e o proximo passo
        millis_t intervalo = 0;

        // o progresso desse passo, enviado para o app
        millis_t progresso = 0;

        // o gcode desse passo
        char gcode[64] = {};
    };

    Passo& passo_atual();

private:
    // id da receita, usado pelo app
    size_t m_id = 0;

    // o passo do escaldo, caso a receita possua um
    std::optional<Passo> m_escaldo;

    // se o escaldo já foi feito
    bool m_escaldou = false;

    // o tempo total que essa receita consumirá
    millis_t m_tempo_total = 0;

    // o tempo de finalização dessa receita
    millis_t m_finalizacao = 0;

    static constexpr auto MAX_ATAQUES = 10; // foi decidido por marcel
    // lista de todos os nossos ataques
    size_t m_num_ataques = 0;
    std::array<Passo, MAX_ATAQUES> m_ataques = {};

    // o passo atual que estamos da receita
    size_t m_passo = 0;
};
}
