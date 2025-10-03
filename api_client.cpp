#include "api_client.h"
#include "config.h"
#include "curl_utils.h"
#include "tools.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <stdexcept>
#include <memory>
#include <unordered_set>

// Helper function to get OpenRouter API key
static std::string get_openrouter_api_key() {
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

ApiClient::ApiClient(UserInterface& ui_ref, std::string& active_model_id_ref)
    : ui(ui_ref), active_model_id_ref(active_model_id_ref) {
}

std::string ApiClient::makeApiCall(const std::vector<Message>& context, 
                                   ToolManager& toolManager,
                                   bool use_tools) {
    CURL* curl = nullptr;
    CURLcode res;
    long http_code = 0;
    std::string response_buffer;
    bool retried_with_default_once = false;
    
    while (true) {
        curl = curl_easy_init();
        if (!curl) {
            throw std::runtime_error("Failed to initialize CURL");
        }
        auto curl_guard = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>{curl, curl_easy_cleanup};
        
        std::string api_key = get_openrouter_api_key();
        
        struct curl_slist* headers = nullptr;
        auto headers_guard = std::unique_ptr<struct curl_slist, decltype(&curl_slist_free_all)>{nullptr, curl_slist_free_all};
        
        headers = curl_slist_append(headers, ("Authorization: Bearer " + api_key).c_str());
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, "HTTP-Referer: https://llm-cli.tsatsin.com");
        headers = curl_slist_append(headers, "X-Title: LLM-cli");
        headers_guard.reset(headers);
        
        nlohmann::json payload;
        payload["model"] = this->active_model_id_ref;
        payload["messages"] = nlohmann::json::array();
        
        // --- Secure Conversation History Construction ---
        std::vector<Message> limited_context;
        if (context.size() > 10) {
            limited_context.assign(context.end() - 10, context.end());
        } else {
            limited_context = context;
        }
        
        std::unordered_set<std::string> valid_tool_ids;
        nlohmann::json msg_array = nlohmann::json::array();
        
        for (size_t i = 0; i < limited_context.size(); ++i) {
            const Message& msg = limited_context[i];
            
            if (msg.role == "assistant" && !msg.content.empty() && msg.content.front() == '{') {
                try {
                    auto asst_json = nlohmann::json::parse(msg.content);
                    if (asst_json.contains("tool_calls")) {
                        std::vector<std::string> ids;
                        for (const auto& tc : asst_json["tool_calls"]) {
                            if (tc.contains("id")) ids.push_back(tc["id"].get<std::string>());
                        }
                        
                        bool all_results_present = true;
                        for (const auto& id : ids) {
                            bool found_result = false;
                            for (size_t j = i + 1; j < limited_context.size() && !found_result; ++j) {
                                if (limited_context[j].role != "tool") continue;
                                try {
                                    auto tool_json = nlohmann::json::parse(limited_context[j].content);
                                    found_result = tool_json.contains("tool_call_id") &&
                                                  tool_json["tool_call_id"] == id;
                                } catch (...) { /* Ignore */ }
                            }
                            if (!found_result) {
                                all_results_present = false;
                                break;
                            }
                        }
                        
                        if (!all_results_present) continue;
                        
                        msg_array.push_back({{"role", "assistant"},
                                           {"content", nullptr},
                                           {"tool_calls", asst_json["tool_calls"]}});
                        valid_tool_ids.insert(ids.begin(), ids.end());
                        continue;
                    }
                } catch (...) { /* Ignore */ }
            }
            
            if (msg.role == "tool") {
                try {
                    auto tool_json = nlohmann::json::parse(msg.content);
                    if (tool_json.contains("tool_call_id") &&
                        valid_tool_ids.count(tool_json["tool_call_id"].get<std::string>())) {
                        msg_array.push_back({{"role", "tool"},
                                           {"tool_call_id", tool_json["tool_call_id"]},
                                           {"name", tool_json["name"]},
                                           {"content", tool_json["content"]}});
                    }
                } catch (...) { /* Ignore */ }
                continue;
            }
            
            msg_array.push_back({{"role", msg.role}, {"content", msg.content}});
        }
        
        payload["messages"] = std::move(msg_array);
        // --- End Secure Conversation History Construction ---
        
        if (use_tools) {
            payload["tools"] = toolManager.get_tool_definitions();
            payload["tool_choice"] = "auto";
        }
        
        std::string json_payload = payload.dump();
        response_buffer.clear();
        
        curl_easy_setopt(curl, CURLOPT_URL, api_base.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, json_payload.size());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_buffer);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);
        
        res = curl_easy_perform(curl);
        
        if (res == CURLE_OK) {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        }
        
        // Check for model-specific errors or general API failure
        bool model_potentially_unavailable = false;
        if (res != CURLE_OK) {
            model_potentially_unavailable = true;
        } else if (http_code == 404 || http_code == 429 || http_code == 500) {
            model_potentially_unavailable = true;
        }
        
        if (model_potentially_unavailable && !retried_with_default_once && this->active_model_id_ref != DEFAULT_MODEL_ID) {
            std::string failed_model_id = this->active_model_id_ref;
            ui.displayError("API call with model '" + failed_model_id + "' failed (Error: " + 
                          (res != CURLE_OK ? curl_easy_strerror(res) : "HTTP " + std::to_string(http_code)) + 
                          "). Attempting to switch to default model: " + std::string(DEFAULT_MODEL_ID));
            
            this->active_model_id_ref = DEFAULT_MODEL_ID;
            ui.displayStatus("Active model set to: " + this->active_model_id_ref);
            retried_with_default_once = true;
            continue;
        }
        
        if (res != CURLE_OK) {
            throw std::runtime_error("API request failed: " + std::string(curl_easy_strerror(res)));
        }
        
        if (http_code != 200) {
            throw std::runtime_error("API request returned HTTP status " + std::to_string(http_code) + ". Response: " + response_buffer);
        }
        
        return response_buffer;
    }
}