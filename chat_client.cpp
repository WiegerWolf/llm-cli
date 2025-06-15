//
// Corrected include order to solve nlohmann::json ambiguity.
// Headers with forward declarations are placed before headers with full definitions.
//
#include "chat_client_api.h"
#include "chat_client_models.h"
#include "chat_client_turn.h"
#include "chat_client.h"

#include "config.h"
#include "database.h"
#include "tools.h"
#include "ui_interface.h"
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

ChatClient::ChatClient(UserInterface& ui_ref, Database& db_ref) : db(db_ref), toolManager(), ui(ui_ref) {
    this->active_model_id = DEFAULT_MODEL_ID;
}
void ChatClient::initialize_model_manager() {
    chat::initialize_model_manager(ui, db, this->active_model_id, this->model_load_future, this->models_loading);
}

std::string ChatClient::makeApiCall(const std::vector<app::db::Message>& context, bool use_tools) {
    nlohmann::json payload;
    payload["model"] = this->active_model_id;
    payload["messages"] = nlohmann::json::array();
    std::unordered_set<std::string> valid_tool_ids;
    nlohmann::json msg_array = nlohmann::json::array();
    for (size_t i = 0; i < context.size(); ++i) {
        const app::db::Message& msg = context[i];
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
                        for (size_t j = i + 1; j < context.size() && !found_result; ++j) {
                            if (context[j].role != "tool") continue;
                            try {
                                auto tool_json = nlohmann::json::parse(context[j].content);
                                found_result = tool_json.contains("tool_call_id") && tool_json["tool_call_id"] == id;
                            } catch (...) {}
                        }
                        if (!found_result) { all_results_present = false; break; }
                    }
                    if (!all_results_present) continue;
                    msg_array.push_back({{"role", "assistant"}, {"content", nullptr}, {"tool_calls", asst_json["tool_calls"]}});
                    valid_tool_ids.insert(ids.begin(), ids.end());
                    continue;
                }
            } catch (...) {}
        }
        if (msg.role == "tool") {
            try {
                auto tool_json = nlohmann::json::parse(msg.content);
                if (tool_json.contains("tool_call_id") && valid_tool_ids.count(tool_json["tool_call_id"].get<std::string>())) {
                    msg_array.push_back({{"role", "tool"}, {"tool_call_id", tool_json["tool_call_id"]}, {"name", tool_json["name"]}, {"content", tool_json["content"]}});
                }
            } catch (...) {}
            continue;
        }
        msg_array.push_back({{"role", msg.role}, {"content", msg.content}});
    }
    payload["messages"] = std::move(msg_array);
    if (use_tools) {
        payload["tools"] = toolManager.get_tool_definitions();
        payload["tool_choice"] = "auto";
    }

    bool retried_with_default_once = false;
    while (true) {
        try {
            return chat::makeApiCall(api_base, payload);
        } catch (const std::runtime_error& e) {
            std::string error_str = e.what();
            bool model_potentially_unavailable = (error_str.find("HTTP status 404") != std::string::npos ||
                                                  error_str.find("HTTP status 429") != std::string::npos ||
                                                  error_str.find("HTTP status 500") != std::string::npos ||
                                                  error_str.find("API request failed") != std::string::npos);

            if (model_potentially_unavailable && !retried_with_default_once && this->active_model_id != DEFAULT_MODEL_ID) {
                ui.displayError("API call with model '" + this->active_model_id + "' failed (" + error_str + "). Attempting to switch to default model: " + std::string(DEFAULT_MODEL_ID));
                this->setActiveModel(DEFAULT_MODEL_ID);
                payload["model"] = this->active_model_id;
                retried_with_default_once = true;
                continue;
            }
            throw;
        }
    }
}
void ChatClient::run() {
    db.cleanupOrphanedToolMessages();
    ui.displayStatus("ChatClient ready. Active model: " + this->active_model_id);
    while (true) {
        try {
            ChatClient& self = *this;
            chat::processTurn(self);
        } catch (const std::runtime_error& e) {
            // runtime_error with "UI signalled exit." is the expected way to exit the loop.
            if (std::string(e.what()) == "UI signalled exit.") {
                break;
            }
            // Other runtime errors are actual problems.
            ui.displayError("Unhandled error in main loop: " + std::string(e.what()));
        }
        catch (...) {
            ui.displayError("An unknown, non-standard error occurred in the main loop.");
        }
    }
}

void ChatClient::setActiveModel(const std::string& model_id) {
    this->active_model_id = model_id;
    ui.displayStatus("ChatClient active model set to: " + model_id);
}

// --- ChatClient Destructor ---
ChatClient::~ChatClient() {
    if (model_load_future.valid()) {
        // If the future is valid, it means the asynchronous task
        // might still be running or hasn't had its result retrieved.
        // We must wait for it to complete to prevent it from accessing
        // members of this ChatClient instance after it's destroyed.
        try {
            model_load_future.wait(); // Safely wait for completion.
        } catch (const std::exception& e) {
            // Optionally log this, but avoid throwing from a destructor.
            // The primary .get() in initialize_model_manager should handle operational errors.
            // For example: std::cerr << "Error during model_load_future cleanup in destructor: " << e.what() << std::endl;
            // Or if ui object is guaranteed to be valid:
            // ui.displayError("Error during model_load_future cleanup in destructor: " + std::string(e.what()));
        } catch (...) {
            // Optionally log generic error.
            // For example: std::cerr << "Unknown error during model_load_future cleanup in destructor." << std::endl;
            // Or if ui object is guaranteed to be valid:
            // ui.displayError("Unknown error during model_load_future cleanup in destructor.");
        }
    }
}
