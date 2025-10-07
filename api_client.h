#pragma once

#include <string>
#include <vector>
#include <functional>
#include <nlohmann/json_fwd.hpp>
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

    // Structure to hold streaming response data
    struct StreamingResponse {
        std::string accumulated_content;
        std::string finish_reason;
        bool has_error = false;
        std::string error_message;
        bool has_tool_calls = false;
        std::string tool_calls_json;  // Accumulated tool_calls as JSON string
        bool callback_exception = false;
        std::string callback_exception_message;
    };

    // Make a streaming API call with the given context
    // Calls the chunk_callback for each content chunk received
    // Returns StreamingResponse with accumulated content and metadata
    // Throws on failure after retry attempts
    StreamingResponse makeStreamingApiCall(const std::vector<Message>& context,
                                          ToolManager& toolManager,
                                          bool use_tools,
                                          const std::function<void(const std::string&)>& chunk_callback);

private:
    UserInterface& ui;
    std::string& active_model_id_ref; // Reference to the active model ID
    std::string api_base = "https://openrouter.ai/api/v1/chat/completions";

    // Helper to build the JSON payload (shared between streaming and non-streaming)
    nlohmann::json buildApiPayload(const std::vector<Message>& context,
                                   ToolManager& toolManager,
                                   bool use_tools,
                                   bool enable_streaming);
};