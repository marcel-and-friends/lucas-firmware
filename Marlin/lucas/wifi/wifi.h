#pragma once

#include <string_view>

namespace lucas::wifi {
// TODO: on_connect callback
void connect(std::string_view network_name, std::string_view network_password);

std::string_view ip();

std::string_view network_name();

std::string_view network_password();
}
