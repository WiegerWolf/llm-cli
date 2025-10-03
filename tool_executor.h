#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "database.h"
#include "ui_interface.h"

// Forward declarations
class PersistenceManager;
class UserInterface;
class ToolManager;
class ApiClient;
class ChatClient;

/**
 * ToolExecutor handles the execution of tool calls:
 * - Standard tool_calls from API responses
 * - Fallback <function> tag parsing and execution
 * - Collecting tool results and making follow-up API calls
 * - Managing the complete tool execution flow
 */
class ToolExecutor {
public:
    explicit ToolExecutor(UserInterface& ui_ref, 
                         PersistenceManager& db_ref,
                         ToolManager& tool_manager_ref,
                         ApiClient& api_client_ref,
                         ChatClient& chat_client_ref,
                         std::string& active_model_id_ref);
    
    // Execute standard tool_calls from API response
    // Returns true if tools were executed and final response obtained
    bool executeStandardToolCalls(const nlohmann::json& response_message,
                                  std::vector<Message>& context);
    
    // Parse and execute fallback <function> tags from content
    // Returns true if any fallback functions were executed
    bool executeFallbackFunctionTags(const std::string& content,
                                     std::vector<Message>& context);

private:
    UserInterface& ui;
    PersistenceManager& db;
    ToolManager& toolManager;
    ApiClient& apiClient;
    ChatClient& chatClient;
    std::string& active_model_id_ref;
    
    // Helper to execute a single tool and prepare result JSON
    std::string executeAndPrepareToolResult(const std::string& tool_call_id,
                                           const std::string& function_name,
                                           const nlohmann::json& function_args);
};