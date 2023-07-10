#include "info.h"
#include <lucas/lucas.h>
#include <lucas/Estacao.h>
#include <lucas/cmd/cmd.h>
#include <lucas/serial/serial.h>
#include <lucas/Fila.h>

namespace lucas::info {
void tick() {
    DocumentoJson doc;
    bool atualizou = false;
    Report::for_each([&](Report& report) {
        const auto tick = millis();
        const auto delta = report.delta(tick);
        if (delta < report.intervalo || !report.callback)
            return util::Iter::Continue;

        if (report.condicao && !report.condicao())
            return util::Iter::Continue;

        report.callback(tick, doc.createNestedObject(report.nome));
        report.ultimo_tick_reportado = tick;
        atualizou = true;

        return util::Iter::Continue;
    });

    if (atualizou)
        print_json(doc);
}

enum ComandoDoApp {
    InicializarEstacoes = 0, // inicializar estacoes
    TempTargetBoiler,        // temperatura target do boiler
    BloquearEstacoes,        // estacoes bloqueadas
    CancelarReceita,         // cancelar receita da estacao
    AgendarReceita,          // enviar uma receita para uma estacao
    RequisicaoDeInfo,        // requisicao de informacao de todas as estacoes
    AgendarReceitaPadrao,    // enviar a receita padrao para uma ou mais estacoes
    MapearReceita,
};

void interpretar_json(std::span<char> buffer) {
    DocumentoJson doc;
    const auto err = deserializeJson(doc, buffer.data(), buffer.size());
    if (err) {
        LOG("desserializacao json falhou - [", err.c_str(), "]");
        return;
    }

    const auto root = doc.as<JsonObjectConst>();
    for (const auto obj : root) {
        const auto chave = obj.key();
        if (chave.size() > 1) {
            LOG("chave invalida - [", chave.c_str(), "]");
            continue;
        }

        const auto comando = ComandoDoApp(*chave.c_str() - '0');
        const auto v = obj.value();
        switch (comando) {
        case InicializarEstacoes: {
            if (!v.is<size_t>()) {
                LOG("valor json invalido para numero de estacoes");
                break;
            }

            Estacao::iniciar(v.as<size_t>());
        } break;
        case TempTargetBoiler: {
            if (!v.is<const char*>()) {
                LOG("valor json invalido para temperatura target do boiler");
                break;
            }

            // cmd::executar_fmt("M140 S%s", v.as<const char*>());
        } break;
        case BloquearEstacoes: {
            if (!v.is<JsonArrayConst>()) {
                LOG("valor json invalido para bloqueamento de bocas");
                break;
            }

            auto array = v.as<JsonArrayConst>();
            if (array.isNull() || array.size() != Estacao::num_estacoes()) {
                LOG("array de estacoes bloqueadas invalido");
                break;
            }

            Estacao::for_each([&array](Estacao& estacao, size_t i) {
                estacao.set_bloqueada(!(array[i].as<bool>()));
                return util::Iter::Continue;
            });
        } break;
        case CancelarReceita: {
            if (!v.is<size_t>() && !v.is<JsonArrayConst>()) {
                LOG("valor json invalido para cancelamento de receita");
                break;
            }

            auto index = v.as<size_t>();
            if (index >= Estacao::num_estacoes()) {
                LOG("index para cancelamento de receita invalido");
                break;
            }

            if (v.is<size_t>()) {
                Fila::the().cancelar_receita_da_estacao(v.as<size_t>());
            } else if (v.is<JsonArrayConst>()) {
                for (auto idx : v.as<JsonArrayConst>()) {
                    Fila::the().cancelar_receita_da_estacao(idx.as<size_t>());
                }
            }
        } break;
        case AgendarReceita: {
            if (!v.is<JsonObjectConst>()) {
                LOG("valor json invalido para envio de uma receita");
                break;
            }

            const auto obj = v.as<JsonObjectConst>();
            Fila::the().agendar_receita(Receita::from_json(obj));
        } break;
        case RequisicaoDeInfo: {
            Fila::the().printar_informacoes();
        } break;
        case AgendarReceitaPadrao: {
            if (!v.is<size_t>() && !v.is<JsonArrayConst>()) {
                LOG("valor json invalido para envio da receita padrao");
                break;
            }

            if (v.is<size_t>()) {
                Fila::the().agendar_receita_para_estacao(Receita::padrao(), v.as<Estacao::Index>());
            } else if (v.is<JsonArrayConst>()) {
                for (auto idx : v.as<JsonArrayConst>()) {
                    Fila::the().agendar_receita_para_estacao(Receita::padrao(), idx.as<Estacao::Index>());
                }
            }
        } break;
        case MapearReceita: {
            if (!v.is<size_t>() && !v.is<JsonArrayConst>()) {
                LOG("valor json invalido para mapear uma receita");
                break;
            }

            if (v.is<size_t>()) {
                Fila::the().mapear_receita_da_estacao(v.as<size_t>());
            } else if (v.is<JsonArrayConst>()) {
                for (auto idx : v.as<JsonArrayConst>()) {
                    Fila::the().mapear_receita_da_estacao(idx.as<size_t>());
                }
            }
        } break;
        default:
            LOG("opcao invalida - [", int(comando), "]");
        }
    }
}

void print_json(const DocumentoJson& doc) {
    SERIAL_CHAR('#');
    serializeJson(doc, SERIAL_IMPL);
    SERIAL_ECHOLNPGM("#");
}
}
