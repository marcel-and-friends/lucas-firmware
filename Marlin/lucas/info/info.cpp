#include "info.h"
#include <lucas/lucas.h>
#include <lucas/core/core.h>
#include <lucas/Station.h>
#include <lucas/cmd/cmd.h>
#include <lucas/serial/serial.h>
#include <lucas/RecipeQueue.h>
#include <lucas/Spout.h>
#include <src/module/temperature.h>

namespace lucas::info {
void setup() {
    if (not CFG(GigaMode)) {
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
    JsonDocument doc;
    bool updated = false;
    Report::for_each([&](Report& report) {
        const auto tick = millis();
        const auto delta = report.delta(tick);
        if (delta < report.interval or not report.callback)
            return util::Iter::Continue;

        if (report.condition and not report.condition())
            return util::Iter::Continue;

        report.callback(doc.createNestedObject(report.nome));
        report.last_reported_tick = tick;
        updated = true;

        return util::Iter::Continue;
    });

    if (updated)
        print_json(doc);
}

enum CommandFromHost {
    InitializeStations = 0, // initialize estacoes
    SetBoilerTemperature,   // temperatura target do boiler
    SetAvailableStations,   // estacoes blockeds
    CancelRecipe,           // cancelar recipe da station
    ScheduleRecipe,         // enviar uma recipe para uma station
    RequestQueueInfo,       // requisicao de informacao de todas as estacoes na fila
    RequestIsCalibrated,    // requisicao de informacao do nivelamento
    UpdateFirmware,         // fixar uma recipe especifica em uma station especifica

    /* ~comandos de desenvolvimento~ */
    ScheduleStandardRecipe, // enviar a recipe standard para uma ou mais estacoes
    SimulateButtonPress,    // simula os efeitos de apertar um button
};

void interpret_json(std::span<char> buffer) {
    JsonDocument doc;
    auto const err = deserializeJson(doc, buffer.data(), buffer.size());
    if (err) {
        LOG_ERR("desserializacao json falhou - [", err.c_str(), "]");
        return;
    }

    auto const root = doc.as<JsonObjectConst>();
    for (auto const obj : root) {
        auto const key = obj.key();
        if (key.size() == 0) {
            LOG_ERR("chave invalida");
            continue;
        }

        auto const v = obj.value();
        auto const command = CommandFromHost(std::stoi(key.c_str()));
        switch (command) {
        case InitializeStations: {
            if (not v.is<size_t>()) {
                LOG_ERR("valor json invalido para numero de estacoes");
                break;
            }

            Station::initialize(v.as<size_t>());
        } break;
        case SetBoilerTemperature: {
            if (not v.is<float>()) {
                LOG_ERR("valor json invalido para temperatura target do boiler");
                break;
            }

            if (not CFG(GigaMode))
                core::calibrate(v.as<float>());
        } break;
        case SetAvailableStations: {
            if (not v.is<JsonArrayConst>()) {
                LOG_ERR("valor json invalido para bloqueamento de bocas");
                break;
            }

            auto array = v.as<JsonArrayConst>();
            if (array.isNull() or array.size() != Station::number_of_stations()) {
                LOG_ERR("array de estacoes blockeds invalido");
                break;
            }

            Station::for_each([&array](Station& station, size_t i) {
                station.set_blocked(not array[i].as<bool>());
                return util::Iter::Continue;
            });
        } break;
        case CancelRecipe: {
            if (not v.is<size_t>() and not v.is<JsonArrayConst>()) {
                LOG_ERR("valor json invalido para cancelamento de recipe");
                break;
            }

            if (v.is<size_t>()) {
                auto index = v.as<size_t>();
                if (index >= Station::number_of_stations()) {
                    LOG_ERR("index para cancelamento de recipe invalido");
                    break;
                }
                RecipeQueue::the().cancel_station_recipe(index);
            } else if (v.is<JsonArrayConst>()) {
                for (auto index : v.as<JsonArrayConst>()) {
                    RecipeQueue::the().cancel_station_recipe(index.as<size_t>());
                }
            }
        } break;
        case ScheduleRecipe: {
            if (not v.is<JsonObjectConst>()) {
                LOG_ERR("valor json invalido para envio de uma recipe");
                break;
            }

            RecipeQueue::the().schedule_recipe(v.as<JsonObjectConst>());
        } break;
        case RequestQueueInfo: {
            if (not v.is<JsonArrayConst>()) {
                LOG_ERR("valor json invalido para requisicao de informacoes");
                break;
            }
            RecipeQueue::the().send_queue_info(v.as<JsonArrayConst>());
        } break;
        case RequestIsCalibrated: {
            if (not core::calibrated())
                core::request_calibration();
        } break;
        case UpdateFirmware: {
            if (not v.is<size_t>()) {
                LOG_ERR("valor json invalido para tamanho do firmware novo");
                break;
            }
            core::prepare_for_firmware_update(v.as<size_t>());
        } break;
        /* ~comandos de desenvolvimento~ */
        case ScheduleStandardRecipe: {
            if (not v.is<size_t>()) {
                LOG_ERR("valor json invalido para envio da recipe standard");
                break;
            }

            for (size_t i = 0; i < v.as<size_t>(); ++i)
                RecipeQueue::the().schedule_recipe(Recipe::standard());

        } break;
        case SimulateButtonPress: {
            if (not v.is<size_t>() and not v.is<JsonArrayConst>()) {
                LOG_ERR("valor json invalido para apertar button");
                break;
            }

            auto apertar_botao = [](size_t index) {
                auto& station = Station::list().at(index);
                if (station.status() == Station::Status::Ready) {
                    station.set_status(Station::Status::Free);
                } else {
                    RecipeQueue::the().map_station_recipe(index);
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
            LOG_ERR("opcao invalida - [", int(command), "]");
        }
    }
}

void print_json(JsonDocument const& doc) {
    SERIAL_CHAR('#');
    serializeJson(doc, SERIAL_IMPL);
    SERIAL_ECHOLNPGM("#");
}
}
