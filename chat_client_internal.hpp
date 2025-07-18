#pragma once

#include "config.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <stdexcept>
#include <vector>

// Forward declarations
struct curl_slist;

namespace chat::detail {

// Declaration only. Definition in chat_client_api.cpp to avoid <iostream> in header.
void handleApiError(const nlohmann::json& error);

// Inline helpers
inline std::string get_openrouter_api_key() {
    constexpr const char* compiled_key = OPENROUTER_API_KEY;
    if (compiled_key[0] != '\0' && std::string(compiled_key) != "@OPENROUTER_API_KEY@") {
        return std::string(compiled_key);
    }
    const char* env_key = std::getenv("OPENROUTER_API_KEY");
    if (env_key) {
        return std::string(env_key);
    }
    throw std::runtime_error("OPENROUTER_API_KEY not set at compile time or in environment");
}

// Custom deleter for stacking CURL pointers in std::unique_ptr
inline void curl_deleter(CURL* curl) {
    if (curl) {
        curl_easy_cleanup(curl);
    }
}

// Custom deleter for stacking curl_slist pointers in std::unique_ptr
inline void slist_deleter(struct curl_slist* list) {
    if (list) {
        curl_slist_free_all(list);
    }
}

// Standard CURL write callback function to append data to a std::string
inline size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t total_size = size * nmemb;
    output->append(static_cast<char*>(contents), total_size);
    return total_size;
}

} // namespace chat::detail