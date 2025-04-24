#pragma once

#include <optional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "database.h" // Includes Message struct
#include "tools.h"    // Includes ToolManager

// Forward declaration needed by ToolManager::execute_tool
class ChatClient; 

// Now define ChatClient fully
class ChatClient {
private:
    PersistenceManager db;
    ToolManager toolManager; 
    std::string api_base = "https://openrouter.ai/api/v1/chat/completions";
    std::string model_name = "openai/gpt-4.1-nano"; 

    std::optional<std::string> promptUserInput();   // lee línea o devuelve nullopt para salir
    void processTurn(const std::string& user_input); // Handles the entire conversation flow for one turn

    // Helper function to execute a tool and prepare the result message JSON string
    std::string executeAndPrepareToolResult(
        const std::string& tool_call_id,
        const std::string& function_name,
        const nlohmann::json& function_args
    );

    /* ------------- helpers extracted from processTurn ------------- */
    void saveUserInput(const std::string& input);

    // Devuelve true si la respuesta era un error que obliga a abortar el turno.
    bool handleApiError(const nlohmann::json& api_response,
                        std::string& fallback_content,
                        nlohmann::json& response_message);

    // Devuelve true si se ejecutó al menos un tool_call estándar.
    bool executeStandardToolCalls(const nlohmann::json& response_message,
                                  std::vector<Message>& context);

    // Devuelve true si el parser fallback <function> ejecutó algo.
    bool executeFallbackFunctionTags(const std::string& content,
                                     std::vector<Message>& context);

    // Imprime y guarda el mensaje assistant “normal”.
    void printAndSaveAssistantContent(const nlohmann::json& response_message);

public:
    // Constructor (if needed, currently default is fine)
    // ChatClient(); 

    // Public method for making API calls (used by web_research tool)
    std::string makeApiCall(const std::vector<Message>& context, bool use_tools = false);

    // Main application loop
    void run();
};
