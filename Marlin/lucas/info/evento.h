#pragma once

#include <ArduinoJson.h>
#include <lucas/util/FixedString.h>

namespace lucas::info {
template<typename I, util::FixedString Str>
class Evento {
public:
    void gerar_json(JsonObject o) const {
        static_cast<const I*>(this)->gerar_json_impl(o);
    }

    const char* nome() const { return Str; }
};
}
