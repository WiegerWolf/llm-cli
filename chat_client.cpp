#include "chat_client.h"
#include "config.h"
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>

// Constructor
ChatClient::ChatClient(UserInterface& ui_ref, PersistenceManager& db_ref)
    : db(db_ref), toolManager(), ui(ui_ref), active_model_id(DEFAULT_MODEL_ID) {
    // Initialize modular components after active_model_id is set
    modelManager = std::make_unique<ModelManager>(ui, db);
    apiClient = std::make_unique<ApiClient>(ui, active_model_id);
    toolExecutor = std::make_unique<ToolExecutor>(ui, db, toolManager, *apiClient, *this, active_model_id);
    commandHandler = std::make_unique<CommandHandler>(ui, db, *modelManager);
}

// Destructor
ChatClient::~ChatClient() = default;

// Initialization
void ChatClient::initialize_model_manager() {
    modelManager->initialize();
    // Sync active_model_id with ModelManager
    active_model_id = modelManager->getActiveModelId();
}

// Main application loop
void ChatClient::run() {
    db.cleanupOrphanedToolMessages();
    ui.displayStatus("ChatClient ready. Active model: " + this->active_model_id);
    
    while (true) {
        try {
            auto input_opt = promptUserInput();
            if (!input_opt) break;
            if (input_opt->empty()) continue;
            processTurn(*input_opt);
        } catch (const std::exception& e) {
            ui.displayError("Unhandled error in main loop: " + std::string(e.what()));
        } catch (...) {
            ui.displayError("An unknown, non-standard error occurred in the main loop.");
        }
    }
}

// Model management delegation
void ChatClient::setActiveModel(const std::string& model_id) {
    modelManager->setActiveModel(model_id);
    active_model_id = model_id;
}

bool ChatClient::are_models_loading() const {
    return modelManager->areModelsLoading();
}

// Public API call method (for tools)
std::string ChatClient::makeApiCall(const std::vector<Message>& context, bool use_tools) {
    return apiClient->makeApiCall(context, toolManager, use_tools);
}

// Private helper methods
std::optional<std::string> ChatClient::promptUserInput() {
    return ui.promptUserInput();
}

void ChatClient::saveUserInput(const std::string& input) {
    db.saveUserMessage(input);
}

bool ChatClient::handleApiError(const nlohmann::json& api_response,
                                std::string& fallback_content,
                                nlohmann::json& response_message) {
    // Check for API Errors first
    if (api_response.contains("error")) {
        ui.displayError("API Error Received: " + api_response["error"].dump(2));
        
        if (api_response["error"].contains("code") &&
            api_response["error"]["code"] == "tool_use_failed" &&
            api_response["error"].contains("failed_generation") &&
            api_response["error"]["failed_generation"].is_string()) {
            fallback_content = api_response["error"]["failed_generation"];
        } else {
            return true; // Unrecoverable API error
        }
    } else if (api_response.contains("choices") && !api_response["choices"].empty() && 
               api_response["choices"][0].contains("message")) {
        response_message = api_response["choices"][0]["message"];
        
        if (response_message.contains("tool_calls") && !response_message["tool_calls"].is_null()) {
            // Standard tool calls path
        } else if (response_message.contains("content") && response_message["content"].is_string()) {
            fallback_content = response_message["content"];
        }
    } else if (api_response.contains("error") &&
               api_response["error"].contains("code") &&
               api_response["error"]["code"] == "tool_use_failed" &&
               api_response["error"].contains("failed_generation") &&
               api_response["error"]["failed_generation"].is_string()) {
        fallback_content = api_response["error"]["failed_generation"];
    } else {
        ui.displayError("Invalid API response structure (First Response). Response was: " + api_response.dump());
        return true;
    }
    return false;
}

void ChatClient::printAndSaveAssistantContent(const nlohmann::json& response_message) {
    if (!response_message.is_null() && response_message.contains("content")) {
        if (response_message["content"].is_string()) {
            std::string txt = response_message["content"];
            db.saveAssistantMessage(txt, this->active_model_id);
            ui.displayOutput(txt + "\n\n", this->active_model_id);
        } else if (!response_message["content"].is_null()) {
            std::string dumped = response_message["content"].dump();
            db.saveAssistantMessage(dumped, this->active_model_id);
            ui.displayOutput(dumped + "\n\n", this->active_model_id);
        }
    }
}

void ChatClient::processTurn(const std::string& input) {
    try {
        // Check for slash commands first
        if (!input.empty() && input[0] == '/') {
            if (commandHandler->handleCommand(input)) {
                return;
            }
        }
        
        // Save user input
        saveUserInput(input);
        
        // Load context
        auto context = db.getContextHistory();
        
        // Make initial API call
        ui.displayStatus("Waiting for response...");
        std::string api_raw_response = apiClient->makeApiCall(context, toolManager, true);
        ui.displayStatus("Processing response...");
        nlohmann::json api_response_json = nlohmann::json::parse(api_raw_response);
        
        // Check for API errors and extract response or fallback content
        std::string fallback_content;
        nlohmann::json response_message;
        if (handleApiError(api_response_json, fallback_content, response_message)) {
            ui.displayStatus("Ready.");
            return;
        }
        
        // Attempt standard tool calls
        bool turn_completed_via_standard_tools = toolExecutor->executeStandardToolCalls(response_message, context);
        
        // Attempt fallback tool tags if standard tools didn't complete
        bool turn_completed_via_fallback_tools = false;
        if (!turn_completed_via_standard_tools && !fallback_content.empty()) {
            turn_completed_via_fallback_tools = toolExecutor->executeFallbackFunctionTags(fallback_content, context);
        }
        
        // If neither path completed, print and save direct response
        if (!turn_completed_via_standard_tools && !turn_completed_via_fallback_tools) {
            printAndSaveAssistantContent(response_message);
        }
        
        ui.displayStatus("Ready.");
        
    } catch (const nlohmann::json::parse_error& e) {
        ui.displayError("Error parsing API response: " + std::string(e.what()));
        ui.displayStatus("Error.");
    } catch (const std::runtime_error& e) {
        ui.displayError("Runtime error: " + std::string(e.what()));
        ui.displayStatus("Error.");
    } catch (const std::exception& e) {
        ui.displayError("An unexpected error occurred: " + std::string(e.what()));
        ui.displayStatus("Error.");
    } catch (...) {
        ui.displayError("An unknown, non-standard error occurred.");
        ui.displayStatus("Error.");
    }
}
