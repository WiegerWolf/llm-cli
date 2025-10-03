#pragma once

#include <string>
#include <vector>
#include "database.h"
#include "ui_interface.h"

// Forward declarations
class PersistenceManager;
class UserInterface;
class ToolManager;

/**
 * ApiClient handles all communication with the OpenRouter API:
 * - Constructing API requests with context and tools
 * - Managing CURL operations
 * - Handling API responses and errors
 * - Implementing retry logic with fallback models
 */
class ApiClient {
public:
    explicit ApiClient(UserInterface& ui_ref, std::string& active_model_id_ref);
    
    // Make an API call with the given context and optional tool definitions
    // Returns the raw JSON response string
    // Throws on failure after retry attempts
    std::string makeApiCall(const std::vector<Message>& context, 
                           ToolManager& toolManager,
                           bool use_tools = false);

private:
    UserInterface& ui;
    std::string& active_model_id_ref; // Reference to the active model ID
    std::string api_base = "https://openrouter.ai/api/v1/chat/completions";
};