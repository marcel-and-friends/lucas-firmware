#include "info.h"
#include <lucas/lucas.h>
#include <lucas/core/core.h>
#include <lucas/Estacao.h>
#include <lucas/cmd/cmd.h>
#include <lucas/serial/serial.h>
#include <lucas/Fila.h>
#include <lucas/Bico.h>
#include <src/module/temperature.h>

namespace lucas::info {
void setup() {
    if (not CFG(ModoGiga)) {
        Report::make(
            "infoBoiler",
            5'000,
            [](JsonObject obj) {
                obj["tempAtual"] = thermalManager.degBed();
            });
    }
    Report::make(
        "boaNoite",
        5'000,
        [](JsonObject obj) {
            obj["oi"] = "boa noite";
        });
}

void tick() {
    DocumentoJson doc;
    bool atualizou = false;
    Report::for_each([&](Report& report) {
        const auto tick = millis();
        const auto delta = report.delta(tick);
        if (delta < report.intervalo or not report.callback)
            return util::Iter::Continue;

        if (report.condicao and not report.condicao())
            return util::Iter::Continue;

        report.callback(doc.createNestedObject(report.nome));
        report.ultimo_tick_reportado = tick;
        atualizou = true;

        return util::Iter::Continue;
    });

    if (atualizou)
        print_json(doc);
}

enum ComandoDoApp {
    InicializarEstacoes = 0,     // inicializar estacoes
    TempTargetBoiler,            // temperatura target do boiler
    BloquearEstacoes,            // estacoes bloqueadas
    CancelarReceita,             // cancelar receita da estacao
    AgendarReceita,              // enviar uma receita para uma estacao
    RequisicaoDeInfoFila,        // requisicao de informacao de todas as estacoes na fila
    RequisicaoDeNivelamento,     // requisicao de informacao do nivelamento
    InformarTamanhoFirmwareNovo, // fixar uma receita especifica em uma estacao especifica

    /* ~comandos de desenvolvimento~ */
    AgendarReceitaPadrao, // enviar a receita padrao para uma ou mais estacoes
    ApertarBotao,         // simula os efeitos de apertar um botao
};

void interpretar_json(std::span<char> buffer) {
    DocumentoJson doc;
    auto const err = deserializeJson(doc, buffer.data(), buffer.size());
    if (err) {
        LOG_ERR("desserializacao json falhou - [", err.c_str(), "]");
        return;
    }

    auto const root = doc.as<JsonObjectConst>();
    for (auto const obj : root) {
        auto const chave = obj.key();
        if (chave.size() > 1) {
            LOG_ERR("chave invalida - [", chave.c_str(), "]");
            continue;
        }

        auto const v = obj.value();
        auto const comando = ComandoDoApp(*chave.c_str() - '0');
        switch (comando) {
        case InicializarEstacoes: {
            if (not v.is<size_t>()) {
                LOG_ERR("valor json invalido para numero de estacoes");
                break;
            }

            Estacao::inicializar(v.as<size_t>());
        } break;
        case TempTargetBoiler: {
            if (not v.is<float>()) {
                LOG_ERR("valor json invalido para temperatura target do boiler");
                break;
            }

            if (not CFG(ModoGiga))
                core::nivelar(v.as<float>());
        } break;
        case BloquearEstacoes: {
            if (not v.is<JsonArrayConst>()) {
                LOG_ERR("valor json invalido para bloqueamento de bocas");
                break;
            }

            auto array = v.as<JsonArrayConst>();
            if (array.isNull() or array.size() != Estacao::num_estacoes()) {
                LOG_ERR("array de estacoes bloqueadas invalido");
                break;
            }

            Estacao::for_each([&array](Estacao& estacao, size_t i) {
                estacao.set_bloqueada(not(array[i].as<bool>()));
                return util::Iter::Continue;
            });
        } break;
        case CancelarReceita: {
            if (not v.is<size_t>() and not v.is<JsonArrayConst>()) {
                LOG_ERR("valor json invalido para cancelamento de receita");
                break;
            }

            if (v.is<size_t>()) {
                auto index = v.as<size_t>();
                if (index >= Estacao::num_estacoes()) {
                    LOG_ERR("index para cancelamento de receita invalido");
                    break;
                }
                Fila::the().cancelar_receita_da_estacao(index);
            } else if (v.is<JsonArrayConst>()) {
                for (auto index : v.as<JsonArrayConst>()) {
                    Fila::the().cancelar_receita_da_estacao(index.as<size_t>());
                }
            }
        } break;
        case AgendarReceita: {
            if (not v.is<JsonObjectConst>()) {
                LOG_ERR("valor json invalido para envio de uma receita");
                break;
            }

            Fila::the().agendar_receita(v.as<JsonObjectConst>());
        } break;
        case RequisicaoDeInfoFila: {
            if (not v.is<JsonArrayConst>()) {
                LOG_ERR("valor json invalido para requisicao de informacoes");
                break;
            }
            Fila::the().gerar_informacoes_da_fila(v.as<JsonArrayConst>());
        } break;
        case RequisicaoDeNivelamento: {
            if (not core::nivelado())
                core::solicitar_nivelamento();
        } break;
        case InformarTamanhoFirmwareNovo: {
            if (not v.is<size_t>()) {
                LOG_ERR("valor json invalido para tamanho do firmware novo");
                break;
            }
            core::prepare_for_firmware_update(v.as<size_t>());
        } break;
        /* ~comandos de desenvolvimento~ */
        case AgendarReceitaPadrao: {
            if (not v.is<size_t>()) {
                LOG_ERR("valor json invalido para envio da receita padrao");
                break;
            }

            for (size_t i = 0; i < v.as<size_t>(); ++i)
                Fila::the().agendar_receita(Receita::padrao());

        } break;
        case ApertarBotao: {
            if (not v.is<size_t>() and not v.is<JsonArrayConst>()) {
                LOG_ERR("valor json invalido para apertar botao");
                break;
            }

            auto apertar_botao = [](size_t index) {
                auto& estacao = Estacao::lista().at(index);
                if (estacao.status() == Estacao::Status::Pronto) {
                    estacao.set_status(Estacao::Status::Livre);
                } else {
                    Fila::the().mapear_receita_da_estacao(index);
                }
            };

            if (v.is<size_t>()) {
                apertar_botao(v.as<size_t>());
            } else if (v.is<JsonArrayConst>()) {
                for (auto index : v.as<JsonArrayConst>()) {
                    apertar_botao(index);
                }
            }
        } break;
        default:
            LOG_ERR("opcao invalida - [", int(comando), "]");
        }
    }
}

void print_json(DocumentoJson const& doc) {
    SERIAL_CHAR('#');
    serializeJson(doc, SERIAL_IMPL);
    SERIAL_ECHOLNPGM("#");
}
}
