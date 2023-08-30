#include "cfg.h"
#include <lucas/lucas.h>
#include <lucas/util/SD.h>

namespace lucas::cfg {
constexpr auto DEFAULT_OPTIONS = std::to_array<Option>({
    [LogPour] = {.id = 'D',  .active = true },
    [LogTravel] = { .id = 'V', .active = true },
    [LogQueue] = { .id = 'F', .active = true },
    [LogCalibration] = { .id = 'N', .active = true },
    [LogStations] = { .id = 'E', .active = true },

    [LogSerial] = { .id = 'S', .active = false},
    [LogWifi] = { .id = 'W', .active = false},
    [LogGcode] = { .id = 'G', .active = false},
    [LogLn] = { .id = 'L', .active = false},

    [GigaMode] = { .id = 'M', .active = false},

    [SetTargetTemperatureOnCalibration] = { .id = 'T', .active = true },
    [FillDigitalSignalTableOnCalibration] = { .id = 'X', .active = true },
});

consteval bool doesnt_have_duplicated_ids(const OptionList& opcoes) {
    for (usize i = 1; i < opcoes.size(); ++i)
        for (usize j = 0; j < i; ++j)
            if (opcoes[i].id != Option::ID_DEFAULT and opcoes[j].id != Option::ID_DEFAULT)
                if (opcoes[i].id == opcoes[j].id)
                    return false;

    return true;
}

static_assert(DEFAULT_OPTIONS.size() == Options::Count, "tamanho errado irmao");
static_assert(doesnt_have_duplicated_ids(DEFAULT_OPTIONS), "opcoes duplicadas irmao");

// vai ser propriamente inicializado na 'setup()'
static OptionList s_options = {};
constexpr auto CFG_FILE_PATH = "cfg.txt";

void setup() {
    auto sd = util::SD::make();
    if (not sd.file_exists(CFG_FILE_PATH)) {
        s_options = DEFAULT_OPTIONS;
        sd.open(CFG_FILE_PATH, util::SD::OpenMode::Write);
        sd.write_from(s_options);
        LOG("nao tem cfg salva, salvou a default");
    } else {
        sd.open(CFG_FILE_PATH, util::SD::OpenMode::Read);
        sd.read_into(s_options);
        LOG("cfg lida do cartao");
    }
}

void save_options() {
    auto sd = util::SD::make();
    sd.open(CFG_FILE_PATH, util::SD::OpenMode::Write);
    sd.write_from(s_options);
}

void reset_options() {
    s_options = DEFAULT_OPTIONS;
    util::SD::make().remove_file(CFG_FILE_PATH);
}

Option get(Options option) {
    return s_options[usize(option)];
}

OptionList& opcoes() {
    return s_options;
}
}
