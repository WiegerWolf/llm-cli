#pragma once

#include <string>
#include <nlohmann/json_fwd.hpp>

namespace chat {

std::string get_openrouter_api_key();
std::string makeApiCall(const std::string& endpoint,
                         const nlohmann::json& payload);
void handleApiError(const nlohmann::json& error);

} // namespace chat