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

    void agendar_receita(JsonObjectConst receita_json);

    void agendar_receita_para_estacao(Receita&, size_t);

    void mapear_receita_da_estacao(size_t);

    void cancelar_receita_da_estacao(size_t);

    void printar_informacoes();

    size_t estacao_ativa() const { return m_estacao_executando; }

    bool executando() const { return m_estacao_executando != Estacao::INVALIDA; }

public:
    void for_each_receita_mapeada(util::IterCallback<Receita&> auto&& callback, const Receita* excecao = nullptr) {
        if (!m_num_receitas) [[likely]]
            return;

        for (auto& info : m_fila) {
            if (!info.ativa || (excecao && &info.receita == excecao) || !(info.receita.passos_pendentes_estao_mapeados()))
                continue;

            if (std::invoke(FWD(callback), info.receita) == util::Iter::Break)
                return;
        }
    };

    void for_each_receita_mapeada(util::IterCallback<const Receita&> auto&& callback, const Receita* excecao = nullptr) const {
        if (!m_num_receitas) [[likely]]
            return;

        for (auto& info : m_fila) {
            if (!info.ativa || (excecao && &info.receita == excecao) || !(info.receita.passos_pendentes_estao_mapeados()))
                continue;

            if (std::invoke(FWD(callback), info.receita) == util::Iter::Break)
                return;
        }
    };

    void for_each_receita_mapeada(util::IterCallback<Receita&, size_t> auto&& callback, const Receita* excecao = nullptr) {
        if (!m_num_receitas) [[likely]]
            return;

        for (size_t i = 0; i < m_fila.size(); ++i) {
            auto& info = m_fila[i];
            if (!info.ativa || (excecao && &info.receita == excecao) || !(info.receita.passos_pendentes_estao_mapeados()))
                continue;

            if (std::invoke(FWD(callback), info.receita, i) == util::Iter::Break)
                return;
        }
    };

    void for_each_receita_mapeada(util::IterCallback<const Receita&, size_t> auto&& callback, const Receita* excecao = nullptr) const {
        if (!m_num_receitas) [[likely]]
            return;

        for (size_t i = 0; i < m_fila.size(); ++i) {
            auto& info = m_fila[i];
            if (!info.ativa || (excecao && &info.receita == excecao) || !(info.receita.passos_pendentes_estao_mapeados()))
                continue;

            if (std::invoke(FWD(callback), info.receita, i) == util::Iter::Break)
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

    void adicionar_receita(size_t);

    void remover_receita(size_t);

    void finalizar_receitas_em_finalizacao();

    void retirar_bico_do_caminho_se_necessario(Receita&, Estacao&);

private:
    size_t m_estacao_executando = Estacao::INVALIDA;

    bool m_executando = false;

    struct ReceitaInfo {
        Receita receita;
        bool ativa = false;
    };

    // isso aqui modela meio que um hashmap mas nao quero alocar
    std::array<ReceitaInfo, Estacao::NUM_MAX_ESTACOES> m_fila = {};
    size_t m_num_receitas = 0;
};
}
