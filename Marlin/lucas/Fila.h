#pragma once

#include <lucas/Estacao.h>
#include <lucas/Receita.h>
#include <ArduinoJson.h>
#include <unordered_map>
#include <vector>

namespace lucas {
class Fila {
public:
    static Fila& the() {
        static Fila instance;
        return instance;
    }

    void tick();

    void agendar_receita(std::unique_ptr<Receita>);

    void agendar_receita_para_estacao(std::unique_ptr<Receita>, Estacao::Index);

    void mapear_receita_da_estacao(Estacao::Index);

    void cancelar_receita_da_estacao(Estacao::Index);

    void printar_informacoes();

    Estacao::Index estacao_ativa() const { return m_estacao_executando; }

    bool executando() const { return m_estacao_executando != Estacao::INVALIDA; }

public:
    void for_each_receita_mapeada(util::IterCallback<Receita&> auto&& callback, const Receita* excecao = nullptr) {
        if (!m_num_receitas) [[likely]]
            return;

        for (auto& receita : m_fila) {
            if (!receita || (excecao && receita.get() == excecao) || !(receita->passos_pendentes_estao_mapeados()))
                continue;

            if (std::invoke(FWD(callback), *receita) == util::Iter::Break)
                return;
        }
    };

    void for_each_receita_mapeada(util::IterCallback<const Receita&> auto&& callback, const Receita* excecao = nullptr) const {
        if (!m_num_receitas) [[likely]]
            return;

        for (auto& receita : m_fila) {
            if (!receita || (excecao && receita.get() == excecao) || !(receita->passos_pendentes_estao_mapeados()))
                continue;

            if (std::invoke(FWD(callback), *receita) == util::Iter::Break)
                return;
        }
    };

    void for_each_receita_mapeada(util::IterCallback<Receita&, Estacao::Index> auto&& callback, const Receita* excecao = nullptr) {
        if (!m_num_receitas) [[likely]]
            return;

        for (size_t i = 0; i < m_fila.size(); ++i) {
            auto& receita = m_fila[i];
            if (!receita || (excecao && receita.get() == excecao) || !(receita->passos_pendentes_estao_mapeados()))
                continue;

            if (std::invoke(FWD(callback), *receita, i) == util::Iter::Break)
                return;
        }
    };

    void for_each_receita_mapeada(util::IterCallback<const Receita&, Estacao::Index> auto&& callback, const Receita* excecao = nullptr) const {
        if (!m_num_receitas) [[likely]]
            return;

        for (size_t i = 0; i < m_fila.size(); ++i) {
            auto& receita = m_fila[i];
            if (!receita || (excecao && receita.get() == excecao) || !(receita->passos_pendentes_estao_mapeados()))
                continue;

            if (std::invoke(FWD(callback), *receita, i) == util::Iter::Break)
                return;
        }
    };

public:
    class NovoPasso : public info::Evento<NovoPasso, "novoPasso"> {
    public:
        void gerar_json_impl(JsonObject o) const {
            o["estacao"] = estacao;
            o["receitaId"] = receita_id;
            o["passo"] = novo_passo;
        }

        size_t estacao;
        size_t novo_passo;
        size_t receita_id;
    };

    class InfoEstacoes : public info::Evento<InfoEstacoes, "infoEstacoes"> {
    public:
        void gerar_json_impl(JsonObject obj) const;
        Fila& fila;
    };

    friend class InfoEstacoes;

private:
    void executar_passo_atual(Receita&, Estacao&);

    void mapear_receita(Receita&, Estacao&);

    void compensar_passo_atrasado(Receita&, Estacao&);

    void tentar_remapear_receitas_apos_cancelamento();

    bool possui_colisoes_com_outras_receitas(const Receita&);

    void adicionar_receita(std::unique_ptr<Receita>, Estacao::Index);

    void remover_receita(Estacao::Index);

    void finalizar_receitas_em_notificacao();

private:
    Estacao::Index m_estacao_executando = Estacao::INVALIDA;

    bool m_executando = false;

    // isso aqui modela meio que um hashmap mas nao quero alocar
    std::array<std::unique_ptr<Receita>, Estacao::NUM_MAX_ESTACOES> m_fila = {};
    size_t m_num_receitas = 0;
};
}
