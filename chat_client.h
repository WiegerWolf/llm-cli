#pragma once

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
    std::string api_base = "https://api.groq.com/openai/v1/chat/completions";
    std::string model_name = "llama-3.3-70b-versatile"; 

    // Helper function for tool execution flow
    bool handleToolExecutionAndFinalResponse(
        ToolManager& toolMgr, 
        const std::string& tool_call_id,
        const std::string& function_name,
        const nlohmann::json& function_args,
        std::vector<Message>& context // Pass context by reference to update it
    );

public:
    // Constructor (if needed, currently default is fine)
    // ChatClient(); 

    // Public method for making API calls (used by web_research tool)
    std::string makeApiCall(const std::vector<Message>& context, bool use_tools = false);

    // Main application loop
    void run();
};
