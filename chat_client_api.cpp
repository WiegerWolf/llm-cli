#include "chat_client_api.h"
#include "chat_client_internal.hpp" // Internal helpers

#include <iostream>

#include <curl/curl.h>


namespace chat {

// Definition for the forward-declared error handler.
void detail::handleApiError(const nlohmann::json& error) {
    if (error.contains("message")) {
        std::cerr << "API Error: " << error["message"] << std::endl;
    } else {
        std::cerr << "An unknown API error occurred." << std::endl;
    }
}

std::string makeApiCall(const std::string& endpoint, const nlohmann::json& payload) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }
    auto curl_guard = std::unique_ptr<CURL, decltype(&chat::detail::curl_deleter)>{curl, chat::detail::curl_deleter};

    std::string api_key = chat::detail::get_openrouter_api_key();
    struct curl_slist* headers = nullptr;
    auto headers_guard = std::unique_ptr<struct curl_slist, decltype(&chat::detail::slist_deleter)>{nullptr, chat::detail::slist_deleter};

    headers = curl_slist_append(headers, ("Authorization: Bearer " + api_key).c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "HTTP-Referer: https://llm-cli.tsatsin.com");
    headers = curl_slist_append(headers, "X-Title: LLM-cli");
    headers_guard.reset(headers);

    std::string json_payload = payload.dump();
    std::string response_buffer;

    curl_easy_setopt(curl, CURLOPT_URL, endpoint.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, json_payload.size());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, chat::detail::WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_buffer);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error("API request failed: " + std::string(curl_easy_strerror(res)));
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    if (http_code != 200) {
        throw std::runtime_error("API request returned HTTP status " + std::to_string(http_code) + ". Response: " + response_buffer);
    }

    return response_buffer;
}

void handleApiError(const nlohmann::json& error) {
    chat::detail::handleApiError(error);
}

}