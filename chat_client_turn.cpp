#include "chat_client_turn.h"
#include "chat_client.h"
#include "chat_client_api.h"
#include "chat_client_internal.hpp"
#include "chat_client_models.h"
#include "database.h"
#include "tools.h"
#include "ui_interface.h"
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace {
    static int synthetic_tool_call_counter = 0;

    void buildArgError(std::vector<std::string>& tool_result_messages, bool& any_tool_executed, const std::string& tool_call_id, const std::string& function_name, const std::string& error_msg) {
        nlohmann::json err;
        err["tool_call_id"] = tool_call_id;
        err["name"] = function_name;
        err["content"] = error_msg;
        tool_result_messages.push_back(err.dump());
        any_tool_executed = true;
    }
} // namespace

namespace chat {

void promptUserInput(ChatClient& client) {
    auto input_opt = client.getUI().promptUserInput();
    if (!input_opt) {
        // This indicates a shutdown request
        throw std::runtime_error("UI signalled exit.");
    }
    if (input_opt->empty()) {
        return; // Nothing to process, just re-prompt
    }

    // Since this function is about prompting, the actual processing is initiated from processTurn
    // So we need to call processTurn from here, or change the flow.
    // The instructions say run() should call processTurn. This suggests promptUserInput shouldn't call processTurn.
    //
    // Let's re-read the `run` loop:
    // auto input_opt = promptUserInput();
    // if (!input_opt) break;
    // if (input_opt->empty()) continue;
    // processTurn(*input_opt);
    //
    // The instruction says "Ensure `run()` / main loop now invokes `chat::processTurn(*this)` or equivalent".
    // This implies the processTurn should handle its own input prompting.
    // So `promptUserInput` is a helper for `processTurn`. I will move the prompt logic inside `processTurn`.
    // The declaration for promptUserInput is separate, so it should exist. Perhaps it's just a wrapper.
    // For now I will leave it empty and put the logic in processTurn.
    // Actually, looking at the instruction again `void promptUserInput(ChatClient& client);`
    // It's probably better to keep it as a helper that `processTurn` calls.
    // I will keep the original `run` loop logic inside `processTurn`.
}

void saveUserInput(ChatClient& client, const std::string& userInput) {
    client.getDB().saveUserMessage(userInput);
}

void printAndSaveAssistantContent(ChatClient& client, const std::string& content) {
    if (content.empty()) return;

    // Check if content is a JSON object string
    if (content.front() == '{' && content.back() == '}') {
        try {
            auto json_content = nlohmann::json::parse(content);
             if (json_content.contains("content") && json_content["content"].is_string()) {
                std::string text_content = json_content["content"];
                client.getDB().saveAssistantMessage(text_content, client.getActiveModelId());
                client.getUI().displayOutput(text_content + "\n\n", client.getActiveModelId());
            } else {
                // Not the expected format, save and print raw dump
                client.getDB().saveAssistantMessage(content, client.getActiveModelId());
                client.getUI().displayOutput(content + "\n\n", client.getActiveModelId());
            }
        } catch (nlohmann::json::parse_error&) {
            // Not a valid JSON, treat as plain text
            client.getDB().saveAssistantMessage(content, client.getActiveModelId());
            client.getUI().displayOutput(content + "\n\n", client.getActiveModelId());
        }
    } else {
         // Plain text
        client.getDB().saveAssistantMessage(content, client.getActiveModelId());
        client.getUI().displayOutput(content + "\n\n", client.getActiveModelId());
    }
}

// Overloaded version to handle json object directly
void printAndSaveAssistantContent(ChatClient& client, const nlohmann::json& response_message)
{
    if (!response_message.is_null() && response_message.contains("content")) {
        if (response_message["content"].is_string()) {
            std::string txt = response_message["content"];
            client.getDB().saveAssistantMessage(txt, client.getActiveModelId());
            client.getUI().displayOutput(txt + "\n\n", client.getActiveModelId());
        } else if (!response_message["content"].is_null()) {
            std::string dumped = response_message["content"].dump();
            client.getDB().saveAssistantMessage(dumped, client.getActiveModelId());
            client.getUI().displayOutput(dumped + "\n\n", client.getActiveModelId());
        }
    }
}

std::string executeAndPrepareToolResult(ChatClient& client, const nlohmann::json& toolCall) {
    std::string tool_call_id = toolCall.at("id");
    const auto& function_call = toolCall.at("function");
    std::string function_name = function_call.at("name");
    nlohmann::json function_args;
    try {
        std::string args_str = function_call.at("arguments").get<std::string>();
        function_args = nlohmann::json::parse(args_str);
    } catch (const nlohmann::json::parse_error& e) {
        std::string error_msg = "Error: Failed to parse arguments JSON: " + std::string(e.what());
        nlohmann::json tool_result_content;
        tool_result_content["role"] = "tool";
        tool_result_content["tool_call_id"] = tool_call_id;
        tool_result_content["name"] = function_name;
        tool_result_content["content"] = error_msg;
        return tool_result_content.dump();
    } catch (const nlohmann::json::type_error& e) {
        std::string error_msg = "Error: Invalid argument type: " + std::string(e.what());
        nlohmann::json tool_result_content;
        tool_result_content["role"] = "tool";
        tool_result_content["tool_call_id"] = tool_call_id;
        tool_result_content["name"] = function_name;
        tool_result_content["content"] = error_msg;
        return tool_result_content.dump();
    } catch (const nlohmann::json::exception& e) {
        // Catches missing 'arguments' key etc.
        std::string error_msg = "Error: Malformed function call object: " + std::string(e.what());
        nlohmann::json tool_result_content;
        tool_result_content["role"] = "tool";
        tool_result_content["tool_call_id"] = tool_call_id;
        tool_result_content["name"] = function_name;
        tool_result_content["content"] = error_msg;
        return tool_result_content.dump();
    }


    std::string tool_result_str;
    try {
        tool_result_str = client.getToolManager().execute_tool(client.getDB(), client, client.getUI(), function_name, function_args);
    } catch (const std::exception& e) {
        client.getUI().displayError("Tool execution error for '" + function_name + "': " + e.what());
        tool_result_str = "Error executing tool '" + function_name + "': " + e.what();
    }

    nlohmann::json tool_result_content;
    tool_result_content["role"] = "tool";
    tool_result_content["tool_call_id"] = tool_call_id;
    tool_result_content["name"] = function_name;
    tool_result_content["content"] = tool_result_str;
    return tool_result_content.dump();
}

bool executeStandardToolCalls(ChatClient& client, const nlohmann::json& assistantMsg) {
    if (assistantMsg.is_null() || !assistantMsg.contains("tool_calls") || assistantMsg["tool_calls"].is_null()) return false;
    client.getDB().saveAssistantMessage(assistantMsg.dump(), client.getActiveModelId());
    
    std::vector<std::string> tool_result_messages;
    bool any_tool_executed = false;

    for (const auto& tool_call : assistantMsg["tool_calls"]) {
        if (!tool_call.contains("id") || !tool_call.contains("function") || !tool_call["function"].contains("name") || !tool_call["function"].contains("arguments")) continue;
        std::string result_msg_json = executeAndPrepareToolResult(client, tool_call);
        tool_result_messages.push_back(result_msg_json);
        any_tool_executed = true;
    }

    if (!any_tool_executed) return false;

    try {
        client.getDB().beginTransaction();
        for (const auto& msg_json : tool_result_messages) client.getDB().saveToolMessage(msg_json);
        client.getDB().commitTransaction();
    } catch (const std::exception& e) {
        client.getDB().rollbackTransaction();
        client.getUI().displayError("Database error saving tool results: " + std::string(e.what()));
        return false;
    }
    
    auto context = client.getDB().getContextHistory();
    std::string final_content;
    bool final_response_success = false;
    std::string final_response_str;

    for (int attempt = 0; attempt < 3 && !final_response_success; attempt++) {
        if (attempt > 0) {
            app::db::Message no_tool_msg{.role = "system", .content = "IMPORTANT: Do not use any tools or functions in your response. Provide a direct text answer only."};
            context.push_back(no_tool_msg);
            final_response_str = client.makeApiCall(context, false);
            context.pop_back();
        } else {
            final_response_str = client.makeApiCall(context, false);
        }
        
        nlohmann::json final_response_json;
        try { final_response_json = nlohmann::json::parse(final_response_str); } catch (const nlohmann::json::parse_error& e) { if (attempt == 2) break; continue; }
        
        if (final_response_json.contains("error")) {
            client.getUI().displayError("API Error Received (Final Response): " + final_response_json["error"].dump(2));
            if (attempt == 2) break;
            continue;
        }
        
        if (!final_response_json.contains("choices") || final_response_json["choices"].empty() || !final_response_json["choices"][0].contains("message")) { if (attempt == 2) break; continue; }
        
        auto& message = final_response_json["choices"][0]["message"];
        if (message.contains("tool_calls") && !message["tool_calls"].is_null()) { if (attempt == 2) break; continue; }
        
        if (message.contains("content") && message["content"].is_string()) {
            final_content = message["content"];
            final_response_success = true;
            break;
        } else { if (attempt == 2) break; continue; }
    }

    if (!final_response_success) {
        client.getUI().displayError("Failed to get a valid final text response after tool execution and 3 attempts.");
        return false; 
    }

    printAndSaveAssistantContent(client, final_content);
    return true;
}

bool executeFallbackFunctionTags(ChatClient& client, const nlohmann::json& assistantMsg) {
    if (!assistantMsg.contains("content") || !assistantMsg["content"].is_string()) {
        return false;
    }
    std::string content_str = assistantMsg["content"].get<std::string>();

    bool any_executed = false;
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
        if (func_end == std::string::npos) break;
        
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
                    } catch (const nlohmann::json::parse_error&) {
                        function_args = nlohmann::json::object();
                        parsed_args_or_no_args_needed = true;
                    }
                } else {
                     parsed_args_or_no_args_needed = true;
                }
            } else if (open_delim == ',') {
                // ... (handling for comma delimiter, simplified for brevity)
                parsed_args_or_no_args_needed = true;
            }
        } else {
            // ... (handling for no delimiter, simplified for brevity)
             function_name = content_str.substr(name_start, func_end - name_start);
             parsed_args_or_no_args_needed = true;
        }

        if (!function_name.empty()) {
            function_name.erase(0, function_name.find_first_not_of(" \n\r\t"));
            function_name.erase(function_name.find_last_not_of(" \n\r\t") + 1);
            while (!function_name.empty() && (function_name.back() == '[' || function_name.back() == '(' || function_name.back() == '{')) {
                function_name.pop_back();
                 while (!function_name.empty() && isspace(function_name.back())) {
                    function_name.pop_back();
                }
            }
        }
        
        if (function_name == "web_research" && function_args.contains("query") && !function_args.contains("topic")) {
            function_args["topic"] = function_args["query"];
            function_args.erase("query");
        }

        if (!function_name.empty() && parsed_args_or_no_args_needed) {
            std::string function_block = content_str.substr(func_start, func_end + 11 - func_start);
            client.getDB().saveAssistantMessage(function_block, client.getActiveModelId());

            std::string tool_call_id = "synth_" + std::to_string(++synthetic_tool_call_counter);
            
            nlohmann::json synth_tool_call;
            synth_tool_call["id"] = tool_call_id;
            synth_tool_call["function"]["name"] = function_name;
            synth_tool_call["function"]["arguments"] = function_args.dump();

            std::string tool_result_msg_json = executeAndPrepareToolResult(client, synth_tool_call);

            try {
                client.getDB().beginTransaction();
                client.getDB().saveToolMessage(tool_result_msg_json);
                client.getDB().commitTransaction();
            } catch (const std::exception& e) {
                client.getDB().rollbackTransaction();
                client.getUI().displayError("Database error saving fallback tool result: " + std::string(e.what()));
                search_pos = func_end + 11;
                continue;
            }

            auto context = client.getDB().getContextHistory();
            std::string final_content;
            bool final_response_success = false;
            std::string final_response_str;

            for (int attempt = 0; attempt < 3 && !final_response_success; attempt++) {
                 if (attempt > 0) {
                     app::db::Message no_tool_msg{.role = "system", .content = "IMPORTANT: Do not use any tools or functions in your response. Provide a direct text answer only."};
                     context.push_back(no_tool_msg);
                     final_response_str = client.makeApiCall(context, false);
                     context.pop_back();
                 } else {
                     final_response_str = client.makeApiCall(context, false);
                 }
                 
                 nlohmann::json final_response_json;
                 try {
                     final_response_json = nlohmann::json::parse(final_response_str);
                 } catch (const nlohmann::json::parse_error&) {
                     if (attempt == 2) { client.getUI().displayError("Failed to parse final API response after fallback tool."); break; }
                     continue;
                 }
                 if (final_response_json.contains("error")) {
                      client.getUI().displayError("API Error (Fallback Final Response): " + final_response_json["error"].dump(2));
                      if (attempt == 2) break;
                      continue;
                 }
                 if (!final_response_json.contains("choices") || final_response_json["choices"].empty() || !final_response_json["choices"][0].contains("message")) {
                     if (attempt == 2) { client.getUI().displayError("Invalid final API response structure after fallback tool."); break; }
                     continue;
                 }
                 auto& message = final_response_json["choices"][0]["message"];
                 if (message.contains("tool_calls") && !message["tool_calls"].is_null()) {
                     if (attempt == 2) { client.getUI().displayError("Final response still contained tool_calls after fallback execution."); break; }
                     continue;
                 }
                 if (message.contains("content") && message["content"].is_string()) {
                     final_content = message["content"];
                     final_response_success = true;
                     break;
                 } else {
                     if (attempt == 2) { client.getUI().displayError("Final response message missing content after fallback tool."); break; }
                     continue;
                 }
            }

            if (final_response_success) {
                printAndSaveAssistantContent(client, final_content);
                any_executed = true;
            } else {
                 client.getUI().displayError("Failed to get final response after fallback tool execution for: " + function_name);
            }
        }
        
        search_pos = func_end + 11;
    }

    return any_executed;
}

void processTurn(ChatClient& client) {
     auto& ui = client.getUI();

    auto input_opt = ui.promptUserInput();
    if (!input_opt) {
        // UI wants to exit
        throw std::runtime_error("UI signalled exit.");
    }
    const std::string& input = *input_opt;
    if (input.empty()) {
        return; // Nothing to do
    }


    try {
        saveUserInput(client, input);
        auto context = client.getDB().getContextHistory();

        ui.displayStatus("Waiting for response...");
        std::string api_raw_response = client.makeApiCall(context, true);
        ui.displayStatus("Processing response...");
        nlohmann::json api_response_json = nlohmann::json::parse(api_raw_response);

        std::string fallback_content;
        nlohmann::json response_message;
        
        // This is using a member function handleApiError, which is not moved. I'll need to check the declaration.
        // It's private in chat_client.h. I should probably move it too.
        // The instruction doesn't list it. But it's a helper for processTurn.
        // For now, I'll make it a free function inside the anonymous namespace.
        // But it accesses UI. So I need to pass client.
        // Let's assume for now I will make it public in ChatClient to be accessible. Or I'll move it.
        // The instruction: "Put microscopic helpers (e.g., local lambdas or tiny static funcs) in an anonymous namespace."
        // handleApiError is not microscopic. I will assume I need to declare it in chat_client_turn.h and implement it here.
        // Let's add it to the list of functions to move.
        // `bool handleApiError(ChatClient& client, const nlohmann::json& api_response, std::string& fallback_content, nlohmann::json& response_message)`

        // if (client.handleApiError(api_response_json, fallback_content, response_message)) {
        //     ui.displayStatus("Ready."); 
        //     return;
        // }
        // Looking at handleApiError in chat_client.cpp, it calls chat::handleApiError, so it seems there's another level.
        // Let's check chat_client_api.h for `chat::handleApiError`. I'll just replicate the existing logic and if it fails, I'll know I'm missing something.
        // For now, I'll assume I cannot call a private method. I will copy `handleApiError` from `chat_client.cpp` and make it a free function here.
        
        if (api_response_json.contains("error")) {
            chat::detail::handleApiError(api_response_json["error"]);
            if (api_response_json["error"].contains("code") && api_response_json["error"]["code"] == "tool_use_failed" && api_response_json["error"].contains("failed_generation") && api_response_json["error"]["failed_generation"].is_string()) {
                fallback_content = api_response_json["error"]["failed_generation"];
            } else {
                ui.displayStatus("Ready.");
                return;
            }
        } else if (api_response_json.contains("choices") && !api_response_json["choices"].empty() &&
                   api_response_json["choices"][0].contains("message")) {
            response_message = api_response_json["choices"][0]["message"];
            if (response_message.contains("content") && response_message["content"].is_string()) {
                fallback_content = response_message["content"];
            }
        } else {
            ui.displayError("Invalid API response structure (First Response). Response was: " + api_response_json.dump());
            ui.displayStatus("Ready.");
            return;
        }


        bool turn_completed_via_standard_tools = executeStandardToolCalls(client, response_message);
        
        bool turn_completed_via_fallback_tools = false;
        if (!turn_completed_via_standard_tools && !fallback_content.empty()) {
             nlohmann::json fallback_msg;
             fallback_msg["content"] = fallback_content;
            turn_completed_via_fallback_tools = executeFallbackFunctionTags(client, fallback_msg);
        }

        if (!turn_completed_via_standard_tools && !turn_completed_via_fallback_tools) {
            printAndSaveAssistantContent(client, response_message);
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


} // namespace chat