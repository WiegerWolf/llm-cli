#pragma once

#include <string>
#include <vector>
#include <optional>
#include <memory>
#include "database.h"
#include "tools.h"
#include "ui_interface.h"
#include "model_manager.h"
#include "api_client.h"
#include "tool_executor.h"
#include "command_handler.h"

// Forward declarations
class ModelManager;
class ApiClient;
class ToolExecutor;
class CommandHandler;

/**
 * ChatClient orchestrates the main conversation flow:
 * - Delegates model management to ModelManager
 * - Delegates API communication to ApiClient
 * - Delegates tool execution to ToolExecutor
 * - Delegates command handling to CommandHandler
 * - Coordinates the overall conversation loop
 */
class ChatClient {
private:
    // Dependencies
    PersistenceManager& db;
    ToolManager toolManager;
    UserInterface& ui;
    
    // Active model state (shared with components)
    std::string active_model_id;
    
    // Modular components (initialized after active_model_id)
    std::unique_ptr<ModelManager> modelManager;
    std::unique_ptr<ApiClient> apiClient;
    std::unique_ptr<ToolExecutor> toolExecutor;
    std::unique_ptr<CommandHandler> commandHandler;
    
    // Private helper methods
    std::optional<std::string> promptUserInput();
    void processTurn(const std::string& user_input);
    void saveUserInput(const std::string& input);
    
    // Handle API errors and extract response or fallback content
    bool handleApiError(const nlohmann::json& api_response,
                        std::string& fallback_content,
                        nlohmann::json& response_message);
    
    // Display and save final assistant content
    void printAndSaveAssistantContent(const nlohmann::json& response_message);

public:
    // Constructor
    explicit ChatClient(UserInterface& ui_ref, PersistenceManager& db_ref);
    ~ChatClient();
    
    // Initialization - must be called before run()
    void initialize_model_manager();
    
    // Main application loop
    void run();
    
    // Model management delegation
    void setActiveModel(const std::string& model_id);

    // Public API call method (for tools)
    std::string makeApiCall(const std::vector<Message>& context, bool use_tools = true);
};
