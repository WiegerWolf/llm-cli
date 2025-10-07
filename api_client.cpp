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
#include <functional>
#include <sstream>

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

nlohmann::json ApiClient::buildApiPayload(const std::vector<Message>& context,
                                          ToolManager& toolManager,
                                          bool use_tools,
                                          bool enable_streaming) {
    nlohmann::json payload;
    payload["model"] = this->active_model_id_ref;
    payload["messages"] = nlohmann::json::array();

    if (enable_streaming) {
        payload["stream"] = true;
    }

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

    return payload;
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

        nlohmann::json payload = buildApiPayload(context, toolManager, use_tools, false);
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

// Structure to hold streaming callback context
struct StreamingCallbackContext {
    std::string buffer;
    ApiClient::StreamingResponse* response;
    const std::function<void(const std::string&)>* chunk_callback;
};

// CURL write callback for streaming responses
static size_t StreamingWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    StreamingCallbackContext* ctx = static_cast<StreamingCallbackContext*>(userp);

    // Append new data to buffer
    ctx->buffer.append(static_cast<char*>(contents), total_size);

    // Process complete lines (SSE format)
    while (true) {
        size_t line_end = ctx->buffer.find('\n');
        if (line_end == std::string::npos) {
            break; // No complete line yet
        }

        std::string line = ctx->buffer.substr(0, line_end);
        ctx->buffer.erase(0, line_end + 1);

        // Trim whitespace
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
            line.pop_back();
        }

        // Skip empty lines
        if (line.empty()) {
            continue;
        }

        // Skip SSE comments (e.g., ": OPENROUTER PROCESSING")
        if (line[0] == ':') {
            continue;
        }

        // Process SSE data lines
        if (line.rfind("data: ", 0) == 0) {
            std::string data = line.substr(6);

            // Check for stream end
            if (data == "[DONE]") {
                break;
            }

            // Parse JSON chunk
            try {
                auto chunk_json = nlohmann::json::parse(data);

                // Check for mid-stream error
                if (chunk_json.contains("error")) {
                    ctx->response->has_error = true;
                    ctx->response->error_message = chunk_json["error"]["message"].get<std::string>();

                    // Check for error finish_reason
                    if (chunk_json.contains("choices") && !chunk_json["choices"].empty()) {
                        auto finish_reason = chunk_json["choices"][0].value("finish_reason", "");
                        if (finish_reason == "error") {
                            ctx->response->finish_reason = "error";
                        }
                    }
                    break;
                }

                // Extract content delta and tool calls
                if (chunk_json.contains("choices") && !chunk_json["choices"].empty()) {
                    const auto& choice = chunk_json["choices"][0];

                    if (choice.contains("delta")) {
                        const auto& delta = choice["delta"];

                        // Handle content
                        if (delta.contains("content") && !delta["content"].is_null()) {
                            std::string content = delta["content"].get<std::string>();
                            ctx->response->accumulated_content += content;

                            // Call the chunk callback
                            if (ctx->chunk_callback) {
                                (*ctx->chunk_callback)(content);
                            }
                        }

                        // Handle tool_calls
                        if (delta.contains("tool_calls") && !delta["tool_calls"].is_null()) {
                            ctx->response->has_tool_calls = true;
                            // Store the entire chunk_json for later processing
                            // We'll need to accumulate these properly
                            if (ctx->response->tool_calls_json.empty()) {
                                ctx->response->tool_calls_json = chunk_json.dump();
                            }
                        }
                    }

                    // Check for finish_reason
                    if (choice.contains("finish_reason") && !choice["finish_reason"].is_null()) {
                        ctx->response->finish_reason = choice["finish_reason"].get<std::string>();
                    }
                }
            } catch (const nlohmann::json::exception& e) {
                // Ignore JSON parsing errors for individual chunks
            }
        }
    }

    return total_size;
}

ApiClient::StreamingResponse ApiClient::makeStreamingApiCall(
    const std::vector<Message>& context,
    ToolManager& toolManager,
    bool use_tools,
    const std::function<void(const std::string&)>& chunk_callback) {

    CURL* curl = nullptr;
    CURLcode res;
    long http_code = 0;
    StreamingCallbackContext callback_ctx;
    StreamingResponse streaming_response;
    callback_ctx.response = &streaming_response;
    callback_ctx.chunk_callback = &chunk_callback;
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

        nlohmann::json payload = buildApiPayload(context, toolManager, use_tools, true);
        std::string json_payload = payload.dump();

        // Reset streaming response for potential retry
        streaming_response = StreamingResponse();
        callback_ctx.buffer.clear();
        callback_ctx.response = &streaming_response;

        curl_easy_setopt(curl, CURLOPT_URL, api_base.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, json_payload.size());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, StreamingWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &callback_ctx);
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
            ui.displayError("Streaming API call with model '" + failed_model_id + "' failed (Error: " +
                          (res != CURLE_OK ? curl_easy_strerror(res) : "HTTP " + std::to_string(http_code)) +
                          "). Attempting to switch to default model: " + std::string(DEFAULT_MODEL_ID));

            this->active_model_id_ref = DEFAULT_MODEL_ID;
            ui.displayStatus("Active model set to: " + this->active_model_id_ref);
            retried_with_default_once = true;
            continue;
        }

        if (res != CURLE_OK) {
            throw std::runtime_error("Streaming API request failed: " + std::string(curl_easy_strerror(res)));
        }

        // For streaming, HTTP 200 is expected even for errors that occur mid-stream
        if (http_code != 200) {
            // Try to parse error from buffer if available
            std::string error_msg = "HTTP " + std::to_string(http_code);
            if (!callback_ctx.buffer.empty()) {
                try {
                    auto error_json = nlohmann::json::parse(callback_ctx.buffer);
                    if (error_json.contains("error") && error_json["error"].contains("message")) {
                        error_msg += ": " + error_json["error"]["message"].get<std::string>();
                    }
                } catch (...) {
                    error_msg += ". Response: " + callback_ctx.buffer;
                }
            }
            throw std::runtime_error("Streaming API request returned " + error_msg);
        }

        // Check if streaming completed with an error
        if (streaming_response.has_error) {
            throw std::runtime_error("Streaming error: " + streaming_response.error_message);
        }

        return streaming_response;
    }
}