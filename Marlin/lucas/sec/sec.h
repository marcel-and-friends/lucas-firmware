#pragma once

#include <lucas/sec/Reason.h>

namespace lucas::sec {
using IdleWhile = bool (*)();

constexpr IdleWhile NO_RETURN = nullptr;

void raise_error(Reason, IdleWhile condition);

bool is_reason_valid(Reason);

void toggle_reason_validity(Reason);
}
