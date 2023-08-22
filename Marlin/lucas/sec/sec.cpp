#include "sec.h"

#include <lucas/util/util.h>
#include <lucas/info/info.h>

namespace lucas::sec {
void raise(Error error) {
    core::TemporaryFilter f{ TickFilter::All };

    info::event("erro", [](JsonObject o) {

    });

    util::idle_until([] { return false; });
}
}
