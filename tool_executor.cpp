#include "tool_executor.h"
#include "tools.h"
#include "api_client.h"
#include "chat_client.h"
#include <stdexcept>
#include <string>

static int synthetic_tool_call_counter = 0;

ToolExecutor::ToolExecutor(UserInterface& ui_ref,
                           PersistenceManager& db_ref,
                           ToolManager& tool_manager_ref,
                           ApiClient& api_client_ref,
                           ChatClient& chat_client_ref,
                           std::string& active_model_id_ref)
    : ui(ui_ref), db(db_ref), toolManager(tool_manager_ref), 
      apiClient(api_client_ref), chatClient(chat_client_ref), 
      active_model_id_ref(active_model_id_ref) {
}

std::string ToolExecutor::executeAndPrepareToolResult(
    const std::string& tool_call_id,
    const std::string& function_name,
    const nlohmann::json& function_args
) {
    std::string tool_result_str;
    try {
        tool_result_str = toolManager.execute_tool(db, chatClient, ui, function_name, function_args);
    } catch (const std::exception& e) {
        ui.displayError("Tool execution error for '" + function_name + "': " + e.what());
        tool_result_str = "Error executing tool '" + function_name + "': " + e.what();
    }
    
    nlohmann::json tool_result_content;
    tool_result_content["role"] = "tool";
    tool_result_content["tool_call_id"] = tool_call_id;
    tool_result_content["name"] = function_name;
    tool_result_content["content"] = tool_result_str;
    
    return tool_result_content.dump();
}

bool ToolExecutor::executeStandardToolCalls(const nlohmann::json& response_message,
                                            std::vector<Message>& context) {
    if (response_message.is_null() || !response_message.contains("tool_calls") || response_message["tool_calls"].is_null()) {
        return false;
    }
    
    // Save the assistant's message requesting tool use
    db.saveAssistantMessage(response_message.dump(), this->active_model_id_ref);
    
    // Execute all tools and collect results
    std::vector<std::string> tool_result_messages;
    bool any_tool_executed = false;
    
    auto buildArgError = [&](const std::string& tool_call_id, const std::string& function_name, const std::string& error_msg) {
        nlohmann::json err;
        err["tool_call_id"] = tool_call_id;
        err["name"] = function_name;
        err["content"] = error_msg;
        tool_result_messages.push_back(err.dump());
        any_tool_executed = true;
    };
    
    for (const auto& tool_call : response_message["tool_calls"]) {
        if (!tool_call.contains("id") || !tool_call.contains("function") || 
            !tool_call["function"].contains("name") || !tool_call["function"].contains("arguments")) {
            continue;
        }
        
        std::string tool_call_id = tool_call["id"];
        std::string function_name = tool_call["function"]["name"];
        nlohmann::json function_args;
        
        try {
            std::string args_str = tool_call["function"]["arguments"].get<std::string>();
            function_args = nlohmann::json::parse(args_str);
        } catch (const nlohmann::json::parse_error& e) {
            buildArgError(tool_call_id, function_name, "Error: Failed to parse arguments JSON: " + std::string(e.what()));
            continue;
        } catch (const nlohmann::json::type_error& e) {
            buildArgError(tool_call_id, function_name, "Error: Invalid argument type: " + std::string(e.what()));
            continue;
        }
        
        std::string result_msg_json = executeAndPrepareToolResult(tool_call_id, function_name, function_args);
        tool_result_messages.push_back(result_msg_json);
        any_tool_executed = true;
    }
    
    if (!any_tool_executed) {
        return false;
    }
    
    // Save all collected tool results to the database
    try {
        db.beginTransaction();
        for (const auto& msg_json : tool_result_messages) {
            db.saveToolMessage(msg_json);
        }
        db.commitTransaction();
    } catch (const std::exception& e) {
        db.rollbackTransaction();
        ui.displayError("Database error saving tool results: " + std::string(e.what()));
        return false;
    }
    
    // Reload context including tool results
    context = db.getContextHistory();
    
    // Make final API call to get text response
    std::string final_content;
    bool final_response_success = false;
    std::string final_response_str;
    
    for (int attempt = 0; attempt < 3 && !final_response_success; attempt++) {
        if (attempt > 0) {
            Message no_tool_msg{"system", "IMPORTANT: Do not use any tools or functions in your response. Provide a direct text answer only."};
            context.push_back(no_tool_msg);
            final_response_str = apiClient.makeApiCall(context, toolManager, false);
            context.pop_back();
        } else {
            final_response_str = apiClient.makeApiCall(context, toolManager, false);
        }
        
        nlohmann::json final_response_json;
        try {
            final_response_json = nlohmann::json::parse(final_response_str);
        } catch (const nlohmann::json::parse_error& e) {
            if (attempt == 2) break;
            continue;
        }
        
        if (final_response_json.contains("error")) {
            ui.displayError("API Error Received (Final Response): " + final_response_json["error"].dump(2));
            if (attempt == 2) break;
            continue;
        }
        
        if (!final_response_json.contains("choices") || final_response_json["choices"].empty() ||
            !final_response_json["choices"][0].contains("message")) {
            if (attempt == 2) break;
            continue;
        }
        
        auto& message = final_response_json["choices"][0]["message"];
        
        if (message.contains("tool_calls") && !message["tool_calls"].is_null()) {
            if (attempt == 2) break;
            continue;
        }
        
        if (message.contains("content") && message["content"].is_string()) {
            final_content = message["content"];
            final_response_success = true;
            break;
        } else {
            if (attempt == 2) break;
            continue;
        }
    }
    
    if (!final_response_success) {
        ui.displayError("Failed to get a valid final text response after tool execution and 3 attempts.");
        return false;
    }
    
    db.saveAssistantMessage(final_content, this->active_model_id_ref);
    ui.displayOutput(final_content + "\n\n", this->active_model_id_ref);
    return true;
}

bool ToolExecutor::executeFallbackFunctionTags(const std::string& content,
                                               std::vector<Message>& context) {
    bool any_executed = false;
    std::string content_str = content;
    size_t search_pos = 0;
    
    while (true) {
        size_t func_start = std::string::npos;
        size_t name_start = std::string::npos;
        const std::string start_tag1 = "<function>";
        const std::string start_tag2 = "<function=";
        
        size_t start_pos1 = content_str.find(start_tag1, search_pos);
        size_t start_pos2 = content_str.find(start_tag2, search_pos);
        
        if (start_pos1 != std::string::npos && (start_pos2 == std::string::npos || start_pos1 < start_pos2)) {
            func_start = start_pos1;
            name_start = func_start + start_tag1.length();
        } else if (start_pos2 != std::string::npos) {
            func_start = start_pos2;
            name_start = func_start + start_tag2.length();
        } else {
            break;
        }
        
        size_t func_end = content_str.find("</function>", name_start);
        if (func_end == std::string::npos) {
            break;
        }
        
        // Argument Parsing Logic
        size_t args_delimiter_start = content_str.find_first_of("{(,", name_start);
        
        if (args_delimiter_start != std::string::npos && args_delimiter_start >= func_end) {
            args_delimiter_start = std::string::npos;
        }
        
        std::string function_name;
        nlohmann::json function_args = nlohmann::json::object();
        bool parsed_args_or_no_args_needed = false;
        
        if (args_delimiter_start != std::string::npos) {
            function_name = content_str.substr(name_start, args_delimiter_start - name_start);
            char open_delim = content_str[args_delimiter_start];
            
            if (open_delim == '{' || open_delim == '(') {
                char close_delim = (open_delim == '{') ? '}' : ')';
                size_t search_end_pos = func_end - 1;
                size_t args_end = content_str.rfind(close_delim, search_end_pos);
                
                if (args_end != std::string::npos && args_end > args_delimiter_start) {
                    std::string args_str = content_str.substr(args_delimiter_start, args_end - args_delimiter_start + 1);
                    try {
                        std::string trimmed_args = args_str;
                        trimmed_args.erase(0, trimmed_args.find_first_not_of(" \n\r\t"));
                        trimmed_args.erase(trimmed_args.find_last_not_of(" \n\r\t") + 1);
                        if (trimmed_args.size() >= 2 && trimmed_args.front() == '(' && trimmed_args.back() == ')') {
                            trimmed_args = trimmed_args.substr(1, trimmed_args.size() - 2);
                            trimmed_args.erase(0, trimmed_args.find_first_not_of(" \n\r\t"));
                            trimmed_args.erase(trimmed_args.find_last_not_of(" \n\r\t") + 1);
                        }
                        function_args = nlohmann::json::parse(trimmed_args);
                        parsed_args_or_no_args_needed = true;
                    } catch (const nlohmann::json::parse_error& e) {
                        function_args = nlohmann::json::object();
                        parsed_args_or_no_args_needed = true;
                    }
                } else {
                    parsed_args_or_no_args_needed = true;
                }
            } else if (open_delim == ',') {
                size_t args_start_pos = args_delimiter_start + 1;
                size_t args_end_pos = func_end;
                if (args_end_pos > args_start_pos) {
                    std::string args_str = content_str.substr(args_start_pos, args_end_pos - args_start_pos);
                    try {
                        function_args = nlohmann::json::parse(args_str);
                        parsed_args_or_no_args_needed = true;
                    } catch (const nlohmann::json::parse_error& e) {
                        function_args = nlohmann::json::object();
                        parsed_args_or_no_args_needed = true;
                    }
                } else {
                    function_args = nlohmann::json::object();
                    parsed_args_or_no_args_needed = true;
                }
            }
        } else {
            size_t brace_pos = content_str.find_first_of("{(", name_start);
            if (brace_pos != std::string::npos && brace_pos < func_end) {
                function_name = content_str.substr(name_start, brace_pos - name_start);
                char open_delim = content_str[brace_pos];
                char close_delim = (open_delim == '{') ? '}' : ')';
                size_t search_end_pos = func_end - 1;
                size_t args_end = content_str.rfind(close_delim, search_end_pos);
                
                if (args_end != std::string::npos && args_end > brace_pos) {
                    std::string args_str = content_str.substr(brace_pos, args_end - brace_pos + 1);
                    try {
                        std::string trimmed_args = args_str;
                        trimmed_args.erase(0, trimmed_args.find_first_not_of(" \n\r\t"));
                        trimmed_args.erase(trimmed_args.find_last_not_of(" \n\r\t") + 1);
                        if (trimmed_args.size() >= 2 && trimmed_args.front() == '(' && trimmed_args.back() == ')') {
                            trimmed_args = trimmed_args.substr(1, trimmed_args.size() - 2);
                            trimmed_args.erase(0, trimmed_args.find_first_not_of(" \n\r\t"));
                            trimmed_args.erase(trimmed_args.find_last_not_of(" \n\r\t") + 1);
                        }
                        function_args = nlohmann::json::parse(trimmed_args);
                        parsed_args_or_no_args_needed = true;
                    } catch (const nlohmann::json::parse_error& e) {
                        function_args = nlohmann::json::object();
                        parsed_args_or_no_args_needed = true;
                    }
                } else {
                    parsed_args_or_no_args_needed = true;
                }
            } else {
                function_name = content_str.substr(name_start, func_end - name_start);
                parsed_args_or_no_args_needed = true;
            }
        }
        
        // Clean up function name
        if (!function_name.empty()) {
            function_name.erase(0, function_name.find_first_not_of(" \n\r\t"));
            function_name.erase(function_name.find_last_not_of(" \n\r\t") + 1);
            while (!function_name.empty() &&
                   (function_name.back() == '[' || function_name.back() == '(' || function_name.back() == '{')) {
                function_name.pop_back();
                while (!function_name.empty() && isspace(function_name.back())) {
                    function_name.pop_back();
                }
            }
        }
        
        // Fix for web_research tool
        if (function_name == "web_research" && function_args.contains("query") && !function_args.contains("topic")) {
            function_args["topic"] = function_args["query"];
            function_args.erase("query");
        }
        
        if (!function_name.empty() && parsed_args_or_no_args_needed) {
            // Recovery attempt for empty args
            if (function_args.empty()) {
                size_t brace_start = content_str.find('{', name_start);
                if (brace_start != std::string::npos && brace_start < func_end) {
                    size_t brace_end = content_str.find('}', brace_start);
                    if (brace_end != std::string::npos && brace_end < func_end) {
                        std::string possible_args = content_str.substr(brace_start, brace_end - brace_start + 1);
                        try {
                            nlohmann::json recovered_args = nlohmann::json::parse(possible_args);
                            function_args = recovered_args;
                        } catch (...) {}
                    }
                }
            }
            
            std::string function_block = content_str.substr(func_start, func_end + 11 - func_start);
            db.saveAssistantMessage(function_block, this->active_model_id_ref);
            context = db.getContextHistory();
            
            std::string tool_call_id = "synth_" + std::to_string(++synthetic_tool_call_counter);
            std::string tool_result_msg_json = executeAndPrepareToolResult(tool_call_id, function_name, function_args);
            
            try {
                db.beginTransaction();
                db.saveToolMessage(tool_result_msg_json);
                db.commitTransaction();
            } catch (const std::exception& e) {
                db.rollbackTransaction();
                ui.displayError("Database error saving fallback tool result: " + std::string(e.what()));
                search_pos = func_end + 11;
                continue;
            }
            
            context = db.getContextHistory();
            
            // Make final API call
            std::string final_content;
            bool final_response_success = false;
            std::string final_response_str;
            
            for (int attempt = 0; attempt < 3 && !final_response_success; attempt++) {
                if (attempt > 0) {
                    Message no_tool_msg{"system", "IMPORTANT: Do not use any tools or functions in your response. Provide a direct text answer only."};
                    context.push_back(no_tool_msg);
                    final_response_str = apiClient.makeApiCall(context, toolManager, false);
                    context.pop_back();
                } else {
                    final_response_str = apiClient.makeApiCall(context, toolManager, false);
                }
                
                nlohmann::json final_response_json;
                try {
                    final_response_json = nlohmann::json::parse(final_response_str);
                } catch (const nlohmann::json::parse_error& e) {
                    if (attempt == 2) { ui.displayError("Failed to parse final API response after fallback tool."); break; }
                    continue;
                }
                
                if (final_response_json.contains("error")) {
                    ui.displayError("API Error (Fallback Final Response): " + final_response_json["error"].dump(2));
                    if (attempt == 2) break;
                    continue;
                }
                
                if (!final_response_json.contains("choices") || final_response_json["choices"].empty() || 
                    !final_response_json["choices"][0].contains("message")) {
                    if (attempt == 2) { ui.displayError("Invalid final API response structure after fallback tool."); break; }
                    continue;
                }
                
                auto& message = final_response_json["choices"][0]["message"];
                
                if (message.contains("tool_calls") && !message["tool_calls"].is_null()) {
                    if (attempt == 2) { ui.displayError("Final response still contained tool_calls after fallback execution."); break; }
                    continue;
                }
                
                if (message.contains("content") && message["content"].is_string()) {
                    final_content = message["content"];
                    final_response_success = true;
                    break;
                } else {
                    if (attempt == 2) { ui.displayError("Final response message missing content after fallback tool."); break; }
                    continue;
                }
            }
            
            if (final_response_success) {
                db.saveAssistantMessage(final_content, this->active_model_id_ref);
                ui.displayOutput(final_content + "\n\n", this->active_model_id_ref);
                any_executed = true;
            } else {
                ui.displayError("Failed to get final response after fallback tool execution for: " + function_name);
            }
        }
        
        search_pos = func_end + 11;
    }
    
    return any_executed;
}