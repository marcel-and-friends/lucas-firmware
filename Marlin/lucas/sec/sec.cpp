#include "sec.h"

#include <lucas/util/util.h>
#include <lucas/info/info.h>

namespace lucas::sec {
void raise(Error error) {
    util::TemporaryFilter f{ Filters::All };

    info::event("erro", [](JsonObject o) {

    });

    util::wait_until([] { return false; });
}
}
