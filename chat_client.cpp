#include "chat_client.h"
#include <iostream>
#include <string>
#include <vector>
#include <curl/curl.h>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <readline/readline.h>
#include <readline/history.h>
#include <unordered_set>   //  NUEVA – para rastrear ids de tool_calls
#include "database.h" // For PersistenceManager and Message
#include "tools.h"    // For ToolManager
#include "config.h"   // For OPENROUTER_API_KEY

static int synthetic_tool_call_counter = 0;

// Bring WriteCallback here as it's used by makeApiCall
// Alternatively, make it a static member of ChatClient or move to a utility file
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t total_size = size * nmemb;
    output->append((char*)contents, total_size);
    return total_size;
}

std::string get_openrouter_api_key() {
    constexpr const char* compiled_key = OPENROUTER_API_KEY;
    if (compiled_key[0] != '\0' && std::string(compiled_key) != "@OPENROUTER_API_KEY@") {
        return std::string(compiled_key);
    }
    const char* env_key = std::getenv("OPENROUTER_API_KEY");
    if (env_key) {
        return std::string(env_key);
    }
    throw std::runtime_error("OPENROUTER_API_KEY not set at compile time or in environment");
}

// --- ChatClient Method Implementations ---

std::string ChatClient::makeApiCall(const std::vector<Message>& context, bool use_tools) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    std::string api_key = get_openrouter_api_key();

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("Authorization: Bearer " + api_key).c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "HTTP-Referer: https://llm-cli.tsatsin.com");
    headers = curl_slist_append(headers, "X-Title: LLM-cli");

    nlohmann::json payload;
    payload["model"] = this->model_name; // Use member variable
    payload["messages"] = nlohmann::json::array();

    // --- armado seguro de la conversación ---------------
    // Garantiza que cada assistant con "tool_calls" incluya TODAS
    // las respuestas «tool» necesarias; de lo contrario se descarta
    std::unordered_set<std::string> valid_tool_ids;   // ids cuyos tool result sí incluiremos
    nlohmann::json msg_array = nlohmann::json::array();

    for (size_t i = 0; i < context.size(); ++i) {
        const Message& msg = context[i];

        /* ---- 1. Mensajes assistant que contienen tool_calls ---- */
        if (msg.role == "assistant" && !msg.content.empty() && msg.content.front() == '{') {
            try {
                auto asst_json = nlohmann::json::parse(msg.content);
                if (asst_json.contains("tool_calls")) {
                    //‑‑ recolectar ids
                    std::vector<std::string> ids;
                    for (const auto& tc : asst_json["tool_calls"])
                        if (tc.contains("id")) ids.push_back(tc["id"].get<std::string>());

                    //‑‑ verificar que cada id tenga su mensaje «tool» más adelante
                    bool all_ok = true;
                    for (const auto& id : ids) {
                        bool found = false;
                        for (size_t j = i + 1; j < context.size() && !found; ++j) {
                            if (context[j].role != "tool") continue;
                            try {
                                auto tool_json = nlohmann::json::parse(context[j].content);
                                found =  tool_json.contains("tool_call_id") &&
                                         tool_json["tool_call_id"] == id;
                            } catch (...) { /* ignorar */ }
                        }
                        if (!found) { all_ok = false; break; }
                    }
                    if (!all_ok) continue;              // DESCARTA este assistant incompleto

                    //‑‑ incluir assistant y marcar ids válidos
                    msg_array.push_back({{"role","assistant"},
                                         {"content",nullptr},
                                         {"tool_calls",asst_json["tool_calls"]}});
                    valid_tool_ids.insert(ids.begin(), ids.end());
                    continue; // Skip generic handling
                }
            } catch (...) { /* no es JSON válido, sigue flujo normal */ }
        }

        /* ---- 2. Mensajes «tool» ---- */
        if (msg.role == "tool") {
            try {
                auto tool_json = nlohmann::json::parse(msg.content);
                if (tool_json.contains("tool_call_id") &&
                    valid_tool_ids.count(tool_json["tool_call_id"].get<std::string>())) {
                    msg_array.push_back({{"role","tool"},
                                         {"tool_call_id",tool_json["tool_call_id"]},
                                         {"name",tool_json["name"]},
                                         {"content",tool_json["content"]}});
                }
            } catch (...) { /* ignorar si está malformado */ }
            continue;   // siempre saltar el manejo genérico para «tool»
        }

        /* ---- 3. user / assistant normales ---- */
        msg_array.push_back({{"role", msg.role}, {"content", msg.content}});
    }

    payload["messages"] = std::move(msg_array);

    // Add tools if requested
    if (use_tools) {
        // Get tool definitions from the ToolManager
        payload["tools"] = toolManager.get_tool_definitions(); 
        payload["tool_choice"] = "auto";
    }

    std::string json_payload = payload.dump();
    std::string response;

    curl_easy_setopt(curl, CURLOPT_URL, api_base.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, json_payload.size());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error("API request failed: " + std::string(curl_easy_strerror(res)));
    }

    // Return the full response string, not just the content, 
    // as we need to check for tool_calls later.
    return response; 
// Executes a single tool and prepares the JSON string for the tool result message.
// Does NOT save to DB or make further API calls.
std::string ChatClient::executeAndPrepareToolResult(
    const std::string& tool_call_id,
    const std::string& function_name,
    const nlohmann::json& function_args
) {
    std::string tool_result_str;
    try {
        // Execute the tool using the ToolManager instance member
        // Pass the db instance needed by some tools like read_history
        // Pass the ChatClient instance (*this) for tools that need to make API calls (like web_research)
        tool_result_str = toolManager.execute_tool(db, *this, function_name, function_args);
    } catch (const std::exception& e) {
         // Errors during argument validation or unknown tool are caught here
         std::cerr << "Tool execution error for '" << function_name << "': " << e.what() << "\n";
         tool_result_str = "Error executing tool '" + function_name + "': " + e.what();
         // Continue to prepare the error as the tool result
    }
    // Note: User feedback like "[Searching web...]" is now handled within toolMgr.execute_tool()

    // Prepare tool result message content as JSON string
    nlohmann::json tool_result_content;
    tool_result_content["tool_call_id"] = tool_call_id;
    tool_result_content["name"] = function_name;
    tool_result_content["content"] = tool_result_str; // Contains result or error message

    // Log the tool result length for debugging
    std::cerr << "Tool result length: " << tool_result_str.length() << " characters" << std::endl;

    // Return the JSON string representation of the tool result message
    return tool_result_content.dump();
}


void ChatClient::run() {
    db.cleanupOrphanedToolMessages();
    std::cout << "Chatting with " << this->model_name
              << " - Type your message (Ctrl+D to exit)\n";
    while (true) {
        auto input_opt = promptUserInput();
        if (!input_opt) break;          // salir con Ctrl‑D
        if (input_opt->empty()) continue;
        processTurn(*input_opt);
    }
}

std::optional<std::string> ChatClient::promptUserInput() {
    char* input_cstr = readline("> ");
    if (!input_cstr) return std::nullopt;       // Ctrl‑D
    std::string input(input_cstr);
    free(input_cstr);
    if (!input.empty()) add_history(input.c_str());
    return input;
}

void ChatClient::saveUserInput(const std::string& input) {
    db.saveUserMessage(input);
}

/* ======== API‑error & fallback ======== */
bool ChatClient::handleApiError(const nlohmann::json& api_response,
                                std::string& fallback_content,
                                nlohmann::json& response_message)
{
    // --- Check for API Errors first ---
    if (api_response.contains("error")) {
        std::cerr << "API Error Received: " << api_response["error"].dump(2) << std::endl;
        // Check for the specific recoverable error
        if (api_response["error"].contains("code") &&
            api_response["error"]["code"] == "tool_use_failed" &&
            api_response["error"].contains("failed_generation") &&
            api_response["error"]["failed_generation"].is_string())
        {
            fallback_content = api_response["error"]["failed_generation"];
            std::cout << "[API reported tool_use_failed, attempting fallback parsing on failed_generation...]\n";
            std::cout.flush();
            // Fall through to the fallback parsing logic below
        } else {
            // Unrecoverable API error, skip processing this turn
            return true;
        }
    // --- Check for standard successful response ---
    } else if (api_response.contains("choices") && !api_response["choices"].empty() && api_response["choices"][0].contains("message")) {
         response_message = api_response["choices"][0]["message"];
         // Check for standard tool calls first
         if (response_message.contains("tool_calls") && !response_message["tool_calls"].is_null()) {
            // Proceed to Path 1: Standard Tool Calls
         }
         // Check for content that might contain fallback syntax
         else if (response_message.contains("content") && response_message["content"].is_string()) {
             fallback_content = response_message["content"];
             // Fall through to the fallback parsing logic below
         }
         // Else: Content is null or not a string, will be handled by the final block
    } else if (api_response.contains("error") &&
               api_response["error"].contains("code") &&
               api_response["error"]["code"] == "tool_use_failed" &&
               api_response["error"].contains("failed_generation") &&
               api_response["error"]["failed_generation"].is_string()) {
        // Additional fallback: handle tool_use_failed error with failed_generation string
        fallback_content = api_response["error"]["failed_generation"];
        std::cout << "[API returned tool_use_failed with failed_generation fallback string, attempting fallback parsing...]\n";
        std::cout.flush();
        // Fall through to the fallback parsing logic below
    } else {
         // Unexpected response structure
         std::cerr << "Error: Invalid API response structure (First Response).\nResponse was: " << api_response.dump() << "\n";
         return true;
    }
    return false;
}

/* ======== tool_calls estándar ======== */
bool ChatClient::executeStandardToolCalls(const nlohmann::json& response_message,
                                          std::vector<Message>& context) // context is IN/OUT
{
    if (response_message.is_null() || !response_message.contains("tool_calls") || response_message["tool_calls"].is_null()) {
        return false; // No standard tool calls to execute
    }

    // 1. Save the assistant's message requesting tool use
    // The content should be the raw JSON string of the message object itself
    db.saveAssistantMessage(response_message.dump());

    // 2. Execute all tools and collect results (without saving yet)
    std::vector<std::string> tool_result_messages; // Store JSON strings of tool messages
    bool any_tool_executed = false;

    for (const auto& tool_call : response_message["tool_calls"]) {
        if (!tool_call.contains("id") || !tool_call.contains("function") || !tool_call["function"].contains("name") || !tool_call["function"].contains("arguments")) {
            std::cerr << "Error: Malformed tool_call object received. Skipping.\n";
            continue; // Skip this malformed tool call
        }
        std::string tool_call_id = tool_call["id"];
        std::string function_name = tool_call["function"]["name"];
        nlohmann::json function_args;
        try {
            // Arguments are expected to be a JSON string that needs parsing
            std::string args_str = tool_call["function"]["arguments"].get<std::string>();
            function_args = nlohmann::json::parse(args_str);
        } catch (const nlohmann::json::parse_error& e) {
            std::cerr << "JSON Parsing Error (Tool Arguments for " << function_name << "): " << e.what() << "\nArgs JSON was: " << tool_call["function"]["arguments"].dump() << "\n";
            // Prepare an error result for this specific tool call
            nlohmann::json error_result_content;
            error_result_content["tool_call_id"] = tool_call_id;
            error_result_content["name"] = function_name;
            error_result_content["content"] = "Error: Failed to parse arguments JSON: " + std::string(e.what());
            tool_result_messages.push_back(error_result_content.dump());
            any_tool_executed = true; // Mark that we attempted execution
            continue; // Skip normal execution for this tool call
        } catch (const nlohmann::json::type_error& e) {
            // Handle cases where arguments might not be a string initially
            std::cerr << "JSON Type Error (Tool Arguments for " << function_name << "): " << e.what() << "\nArgs JSON was: " << tool_call["function"]["arguments"].dump() << "\n";
            // Prepare an error result
            nlohmann::json error_result_content;
            error_result_content["tool_call_id"] = tool_call_id;
            error_result_content["name"] = function_name;
            error_result_content["content"] = "Error: Invalid argument type: " + std::string(e.what());
            tool_result_messages.push_back(error_result_content.dump());
            any_tool_executed = true;
            continue;
        }

        // Call the helper function to execute the tool and get the result message JSON string
        std::string result_msg_json = executeAndPrepareToolResult(tool_call_id, function_name, function_args);
        tool_result_messages.push_back(result_msg_json);
        any_tool_executed = true;
    }

    // If no tools were actually executed (e.g., all malformed), return false
    if (!any_tool_executed) {
        std::cerr << "Warning: Assistant requested tool calls, but none could be executed.\n";
        return false;
    }

    // 3. Save all collected tool results to the database
    try {
        // Use a transaction for atomicity if desired (optional but good practice)
        // db.beginTransaction(); // Assuming a method like this exists or implement exec("BEGIN") etc.
        for (const auto& msg_json : tool_result_messages) {
            db.saveToolMessage(msg_json);
        }
        // db.commitTransaction(); // Assuming commit
    } catch (const std::exception& e) {
        // db.rollbackTransaction(); // Assuming rollback
        std::cerr << "Error saving tool results to database: " << e.what() << std::endl;
        // Decide how to proceed. Maybe return false? For now, log and continue.
        return false; // Indicate failure if DB save fails
    }


    // 4. Reload context INCLUDING the assistant message and ALL tool results
    context = db.getContextHistory();

    // 5. Make the final API call to get the text response
    std::string final_content;
    bool final_response_success = false;

    for (int attempt = 0; attempt < 3 && !final_response_success; attempt++) {
        // On subsequent attempts, add a system message to explicitly prevent tool usage
        if (attempt > 0) {
            context.push_back({"system", "IMPORTANT: Do not use any tools or functions in your response. Provide a direct text answer only."});
        }

        std::string final_response_str = makeApiCall(context, false); // Tools explicitly disabled
        nlohmann::json final_response_json;
        try {
            final_response_json = nlohmann::json::parse(final_response_str);
        } catch (const nlohmann::json::parse_error& e) {
            std::cerr << "JSON Parsing Error (Final Response): " << e.what() << "\nResponse was: " << final_response_str << "\n";
            if (attempt == 2) break; // Exit loop on last attempt
            continue; // Try again
        }

        // Check for API errors in the final response
        if (final_response_json.contains("error")) {
             std::cerr << "API Error Received (Final Response): " << final_response_json["error"].dump(2) << std::endl;
             if (attempt == 2) break;
             continue; // Try again
        }

        // Check if we have a valid response structure
        if (!final_response_json.contains("choices") || final_response_json["choices"].empty() ||
            !final_response_json["choices"][0].contains("message")) {
            std::cerr << "Error: Invalid API response structure (Final Response).\nResponse was: " << final_response_str << "\n";
            if (attempt == 2) break;
            continue; // Try again
        }

        auto& message = final_response_json["choices"][0]["message"];

        // Check if the final message *still* contains tool_calls (should be rare now)
        if (message.contains("tool_calls") && !message["tool_calls"].is_null()) {
            std::cerr << "Warning: Final response unexpectedly contains tool_calls. Retrying with explicit instruction.\n";
            // Optionally save this unexpected assistant message?
            // db.saveAssistantMessage(message.dump());
            if (attempt == 2) break;
            continue; // Try again with stronger instructions
        }

        // If we have content, we're good
        if (message.contains("content") && message["content"].is_string()) {
            final_content = message["content"];
            final_response_success = true;
            break; // Success!
        } else {
            std::cerr << "Error: Final response message missing content field.\nResponse was: " << final_response_str << "\n";
            if (attempt == 2) break;
            continue; // Try again
        }
    }

    // 6. Handle the final response
    if (!final_response_success) {
        std::cerr << "Error: Failed to get a valid final text response after tool execution and 3 attempts.\n";
        return false; // Indicate overall failure for this turn's tool path
    }

    // Save the final assistant response
    db.saveAssistantMessage(final_content);

    // Display final response
    std::cout << final_content << "\n\n";
    std::cout.flush(); // Ensure output is displayed before next prompt
    return true; // Indicate success for the standard tool call path
}

/* ======== parser fallback <function> ======== */
bool ChatClient::executeFallbackFunctionTags(const std::string& content,
                                             std::vector<Message>& context)
{
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
            break; // No more function tags found
        }

        size_t func_end = content_str.find("</function>", name_start);
        if (func_end == std::string::npos) {
            break; // No closing tag found, stop
        }

        // Find the start of arguments: either '{', '(', OR ','
        size_t args_delimiter_start = content_str.find_first_of("{(,", name_start);

        if (args_delimiter_start != std::string::npos && args_delimiter_start >= func_end) {
            args_delimiter_start = std::string::npos; // Treat as no delimiter before end tag
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
                        std::cerr << "Warning: Failed to parse arguments JSON from <function...>: " << e.what() << ". Treating as empty args.\nArgs string was: " << args_str << "\n";
                        function_args = nlohmann::json::object();
                        parsed_args_or_no_args_needed = true;
                    }
                } else {
                    std::cerr << "Warning: Malformed arguments - found '" << open_delim << "' but no matching '" << close_delim << "' before </function>.\n";
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
                        std::cerr << "Warning: Failed to parse arguments JSON after comma in <function...>: " << e.what() << ". Treating as empty args.\nArgs string was: " << args_str << "\n";
                        function_args = nlohmann::json::object();
                        parsed_args_or_no_args_needed = true;
                    }
                } else {
                    std::cerr << "Warning: Found comma delimiter but no arguments before </function>.\n";
                    function_args = nlohmann::json::object();
                    parsed_args_or_no_args_needed = true;
                }
            }
        } else {
            // No explicit delimiter found, but check for special case:
            // function name immediately followed by '(' or '{' (e.g., <function(search_web={"query":...})</function>)
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
                        std::cerr << "Warning: Failed to parse arguments JSON from <function...>: " << e.what() << ". Treating as empty args.\nArgs string was: " << args_str << "\n";
                        function_args = nlohmann::json::object();
                        parsed_args_or_no_args_needed = true;
                    }
                } else {
                    std::cerr << "Warning: Malformed arguments - found '" << open_delim << "' but no matching '" << close_delim << "' before </function>.\n";
                    parsed_args_or_no_args_needed = true;
                }
            } else {
                function_name = content_str.substr(name_start, func_end - name_start);
                parsed_args_or_no_args_needed = true;
            }
        }

        if (!function_name.empty()) {
            function_name.erase(0, function_name.find_first_not_of(" \n\r\t"));
            function_name.erase(function_name.find_last_not_of(" \n\r\t") + 1);

            // Additional cleanup: remove trailing stray characters like '[', '(', '{'
            while (!function_name.empty() && 
                   (function_name.back() == '[' || function_name.back() == '(' || function_name.back() == '{')) {
                function_name.pop_back();
                // Also trim any whitespace after popping
                while (!function_name.empty() && isspace(function_name.back())) {
                    function_name.pop_back();
                }
            }
        }

        // *** FIX: Handle web_research fallback using 'query' instead of 'topic' ***
        if (function_name == "web_research" && function_args.contains("query") && !function_args.contains("topic")) {
            std::cout << "[Fallback parser: Renaming 'query' to 'topic' for web_research]\n"; std::cout.flush();
            function_args["topic"] = function_args["query"];
            function_args.erase("query");
        }

        if (!function_name.empty() && parsed_args_or_no_args_needed) {
            // If args are empty, try to recover by parsing the first {...} block inside the tag
            if (function_args.empty()) {
                size_t brace_start = content_str.find('{', name_start);
                size_t brace_end = content_str.rfind('}', func_end - 1);
                if (brace_start != std::string::npos && brace_end != std::string::npos && brace_end > brace_start) {
                    std::string possible_args = content_str.substr(brace_start, brace_end - brace_start + 1);
                    try {
                        nlohmann::json recovered_args = nlohmann::json::parse(possible_args);
                        function_args = recovered_args;
                    } catch (...) {
                        // Ignore parse errors, keep empty args
                    }
                }
            }

            std::string function_block = content_str.substr(func_start, func_end + 11 - func_start);
            db.saveAssistantMessage(function_block);
            context = db.getContextHistory();

            std::string tool_call_id = "synth_" + std::to_string(++synthetic_tool_call_counter);

            std::cout << "[Executing function from content: " << function_name << "]\n";
            std::cout.flush();

            // Execute tool and get result JSON string
            std::string tool_result_msg_json = executeAndPrepareToolResult(tool_call_id, function_name, function_args);

            // Save the tool result
            try {
                db.saveToolMessage(tool_result_msg_json);
            } catch (const std::exception& e) {
                 std::cerr << "Error saving fallback tool result: " << e.what() << std::endl;
                 search_pos = func_end + 11; // Move past this tag
                 continue; // Skip API call for this failed save
            }

            // Reload context
            context = db.getContextHistory();

            // Make the final API call (similar logic to executeStandardToolCalls's final step)
            // Try up to 3 times
            std::string final_content;
            bool final_response_success = false;
            for (int attempt = 0; attempt < 3 && !final_response_success; attempt++) {
                 if (attempt > 0) {
                     context.push_back({"system", "IMPORTANT: Do not use any tools or functions in your response. Provide a direct text answer only."});
                 }
                 std::string final_response_str = makeApiCall(context, false);
                 nlohmann::json final_response_json;
                 try {
                     final_response_json = nlohmann::json::parse(final_response_str);
                 } catch (const nlohmann::json::parse_error& e) {
                     std::cerr << "JSON Parsing Error (Fallback Final Response): " << e.what() << "\nResponse was: " << final_response_str << "\n";
                     if (attempt == 2) break;
                     continue;
                 }
                 if (final_response_json.contains("error")) {
                      std::cerr << "API Error Received (Fallback Final Response): " << final_response_json["error"].dump(2) << std::endl;
                      if (attempt == 2) break;
                      continue;
                 }
                 if (!final_response_json.contains("choices") || final_response_json["choices"].empty() || !final_response_json["choices"][0].contains("message")) {
                     std::cerr << "Error: Invalid API response structure (Fallback Final Response).\nResponse was: " << final_response_str << "\n";
                     if (attempt == 2) break;
                     continue;
                 }
                 auto& message = final_response_json["choices"][0]["message"];
                 if (message.contains("tool_calls") && !message["tool_calls"].is_null()) {
                     std::cerr << "Warning: Fallback final response unexpectedly contains tool_calls. Retrying.\n";
                     if (attempt == 2) break;
                     continue;
                 }
                 if (message.contains("content") && message["content"].is_string()) {
                     final_content = message["content"];
                     final_response_success = true;
                     break;
                 } else {
                     std::cerr << "Error: Fallback final response message missing content field.\nResponse was: " << final_response_str << "\n";
                     if (attempt == 2) break;
                     continue;
                 }
            }

            if (final_response_success) {
                db.saveAssistantMessage(final_content);
                std::cout << final_content << "\n\n";
                std::cout.flush();
                any_executed = true; // Mark success for this fallback path
            } else {
                 std::cerr << "Error: Failed to get final response after fallback tool execution.\n";
                 // Continue searching for other tags, but this one failed
            }
        }

        search_pos = func_end + 11; // Move past this </function>
    }

    return any_executed; // True if at least one fallback function led to a final response
}

/* ======== print & save assistant ======== */
void ChatClient::printAndSaveAssistantContent(const nlohmann::json& response_message)
{
    if (!response_message.is_null() && response_message.contains("content")) {
        if (response_message["content"].is_string()) {
            std::string txt = response_message["content"];
            db.saveAssistantMessage(txt);
            std::cout << txt << "\n\n";
        } else if (!response_message["content"].is_null()) {
            std::string dumped = response_message["content"].dump();
            db.saveAssistantMessage(dumped);
            std::cout << dumped << "\n\n";
        }
        std::cout.flush();
    }
}

void ChatClient::processTurn(const std::string& input) {
    try {
        saveUserInput(input);
        auto context = db.getContextHistory();

        // 1ª llamada al modelo con tools habilitados
        std::string api_raw   = makeApiCall(context, true);
        nlohmann::json api_js = nlohmann::json::parse(api_raw);

        std::string  fallback_content;
        nlohmann::json response_msg;

        if (handleApiError(api_js, fallback_content, response_msg))
            return;                               // turno terminado por error

        bool tool_done = executeStandardToolCalls(response_msg, context);

        if (!tool_done && !fallback_content.empty())
            tool_done = executeFallbackFunctionTags(fallback_content, context);

        if (!tool_done)
            printAndSaveAssistantContent(response_msg);

    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "JSON Parsing Error (Outer Turn): " << e.what() << "\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
}
