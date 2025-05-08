#pragma once
// #include <stop_token> // Removed for now due to potential C++20 incompatibility

#include <optional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "database.h" // Includes Message struct
#include "tools.h"    // Includes ToolManager
#include "ui_interface.h" // Include the UI abstraction
#include "config.h"       // For OPENROUTER_API_URL_MODELS and DEFAULT_MODEL_ID
#include "model_types.h"  // For ModelData struct
#include <thread>         // For std::thread
#include <atomic>         // For std::atomic

// Forward declaration needed by ToolManager::execute_tool
class ChatClient;

// Now define ChatClient fully
class ChatClient {
private:
    PersistenceManager& db; // Changed to reference
    ToolManager toolManager;
    UserInterface& ui; // Add reference to the UI
    std::string api_base = "https://openrouter.ai/api/v1/chat/completions";
    // std::string model_name = "openai/gpt-4.1-nano"; // Replaced by active_model_id
    
    // --- Model Selection State (Part III GUI Changes) ---
    std::string active_model_id;
    const std::string default_model_id = "phi3:mini"; // Match GuiInterface
    // --- End Model Selection State ---

    // For model initialization
    std::thread model_init_thread;
    std::atomic<bool> models_initialized_successfully{false};
    std::atomic<bool> model_initialization_attempted{false};

    // Methods for model fetching, parsing, and caching
    std::string fetchModelsFromAPI();
    std::vector<ModelData> parseModelsFromAPIResponse(const std::string& api_response);
    void cacheModelsToDB(const std::vector<ModelData>& models);
    void initializeModels(); // Orchestrates fetching, parsing, and caching

    // Prompts the user for input via the UI interface. Returns nullopt if UI signals exit/shutdown.
    std::optional<std::string> promptUserInput();
    // Handles the entire conversation flow for one turn, including API calls and tool execution.
    void processTurn(const std::string& user_input);

    // Helper function to execute a tool and prepare the result message JSON string
    std::string executeAndPrepareToolResult(
        const std::string& tool_call_id,
        const std::string& function_name,
        const nlohmann::json& function_args
    );

    /* ------------- helpers extracted from processTurn ------------- */
    // Saves the user's input message to the database.
    void saveUserInput(const std::string& input);

    // Handles potential errors in the API response. Returns true if an error occurred that forces the turn to abort.
    bool handleApiError(const nlohmann::json& api_response,
                        std::string& fallback_content,
                        nlohmann::json& response_message);

    // Executes standard tool calls requested in the API response. Returns true if at least one standard tool_call was executed.
    bool executeStandardToolCalls(const nlohmann::json& response_message,
                                  std::vector<Message>& context);

    // Executes fallback function calls embedded in <function> tags within the content. Returns true if any fallback function was executed.
    bool executeFallbackFunctionTags(const std::string& content,
                                     std::vector<Message>& context);

    // Displays and saves the "normal" assistant content message.
    void printAndSaveAssistantContent(const nlohmann::json& response_message);

public:
    // Constructor now requires a UserInterface reference
    explicit ChatClient(UserInterface& ui_ref, PersistenceManager& db_ref); // Added db_ref

    // Public method for making API calls (used by web_research tool)
    std::string makeApiCall(const std::vector<Message>& context, bool use_tools = false);

    // --- Model Selection Method (Part III GUI Changes) ---
    void setActiveModel(const std::string& model_id);
    // --- End Model Selection Method ---

    // Main application loop
    // Main application loop
    void run(); // Removed std::stop_token for now
};
