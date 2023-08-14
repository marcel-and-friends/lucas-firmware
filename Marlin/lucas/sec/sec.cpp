#include "sec.h"

#include <lucas/util/util.h>
#include <lucas/info/info.h>

namespace lucas::sec {
void raise(Error error) {
    util::FiltroUpdatesTemporario f{ Filtros::Todos };

    info::evento("erro", [](JsonObject o) {

    });

    util::aguardar_ate([] { return false; });
}
}
