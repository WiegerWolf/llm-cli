#include "chat_client.h"
// #include <stop_token> // Removed for now
#include "ui_interface.h" // Include UI interface
#include "config.h"       // For API URLs, DEFAULT_MODEL_ID
#include <string>
#include <vector>
#include <curl/curl.h>
#include <cstdlib>        // Keep for getenv
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <unordered_set>   // To track valid tool_call IDs during context reconstruction
#include "database.h" // For PersistenceManager and Message
#include "tools.h"    // For ToolManager
#include "curl_utils.h" // Include the shared callback

static int synthetic_tool_call_counter = 0;

// WriteCallback moved to curl_utils.h

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

// --- ChatClient Constructor ---
ChatClient::ChatClient(UserInterface& ui_ref, PersistenceManager& db_ref) :
    db(db_ref),        // Initialize PersistenceManager reference
    toolManager(),     // Initialize ToolManager
    ui(ui_ref)         // Initialize the UI reference
{
    // Constructor body
    // Launch model initialization in a separate thread
    if (model_init_thread.joinable()) {
        model_init_thread.join();
    }
    model_init_thread = std::thread(&ChatClient::initializeModels, this);
    model_init_thread.detach(); // Detach to run in background
}

// --- Model Initialization Methods ---

void ChatClient::initializeModels() {
    model_initialization_attempted = true;
    ui.displayStatus("Initializing models...");

    std::string api_response_str = fetchModelsFromAPI();

    if (api_response_str.empty()) {
        ui.displayStatus("Failed to fetch models from API. Attempting to load from cache...");
        std::vector<ModelData> cached_models = db.getAllModels(); // Needs to be implemented in PersistenceManager

        if (cached_models.empty()) {
            ui.displayError("Failed to load models from cache. Falling back to default model: " + std::string(DEFAULT_MODEL_ID));
            this->model_name = DEFAULT_MODEL_ID; // Ensure model_name is a member that can be set
            models_initialized_successfully = false;
        } else {
            ui.displayStatus("Successfully loaded " + std::to_string(cached_models.size()) + " models from cache.");
            // Optional: Populate an internal list of available models from cached_models
            // For now, we assume the first cached model or a default logic will pick one.
            // If a specific model was previously selected and is in cache, that would be ideal.
            // For simplicity, if DEFAULT_MODEL_ID is in cache, use it, otherwise, maybe the first one.
            bool default_found = false;
            for(const auto& model : cached_models) {
                if (model.id == DEFAULT_MODEL_ID) {
                    this->model_name = DEFAULT_MODEL_ID;
                    default_found = true;
                    break;
                }
            }
            if (!default_found && !cached_models.empty()) {
                 this->model_name = cached_models[0].id; // Fallback to the first cached model
                 ui.displayStatus("Default model not in cache. Using first cached model: " + this->model_name);
            } else if (cached_models.empty()) { // Should not happen due to outer if, but defensive
                 this->model_name = DEFAULT_MODEL_ID;
            }
            models_initialized_successfully = true;
        }
    } else {
        ui.displayStatus("Fetched model data from API. Parsing...");
        std::vector<ModelData> fetched_models = parseModelsFromAPIResponse(api_response_str);

        if (fetched_models.empty()) {
            ui.displayError("Fetched models from API, but failed to parse or no models found. Check API response format. Falling back to default model.");
            // Consider fallback to cache here as well, or just use default.
            // For now, using default as per plan if parse fails.
            this->model_name = DEFAULT_MODEL_ID;
            models_initialized_successfully = false;
        } else {
            ui.displayStatus("Successfully parsed " + std::to_string(fetched_models.size()) + " models from API. Caching to DB...");
            cacheModelsToDB(fetched_models);
            ui.displayStatus("Successfully fetched and cached models from API.");
            // Optional: Populate internal list of available models
            // Set current model (e.g., to default if available, or first fetched)
            bool default_found = false;
            for(const auto& model : fetched_models) {
                if (model.id == DEFAULT_MODEL_ID) {
                    this->model_name = DEFAULT_MODEL_ID;
                    default_found = true;
                    break;
                }
            }
            if (!default_found && !fetched_models.empty()) {
                 this->model_name = fetched_models[0].id; // Fallback to the first fetched model
                 ui.displayStatus("Default model not among fetched. Using first fetched model: " + this->model_name);
            } else if (fetched_models.empty()) { // Should not happen
                 this->model_name = DEFAULT_MODEL_ID;
            }
            models_initialized_successfully = true;
        }
    }
    if (models_initialized_successfully) {
        ui.displayStatus("Model initialization completed. Current model: " + this->model_name);
    } else {
        ui.displayError("Model initialization failed. Current model: " + this->model_name);
    }
}

std::string ChatClient::fetchModelsFromAPI() {
    CURL* curl = curl_easy_init();
    if (!curl) {
        ui.displayError("Failed to initialize CURL for fetching models.");
        return "";
    }
    auto curl_guard = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>{curl, curl_easy_cleanup};

    std::string api_key;
    try {
        api_key = get_openrouter_api_key();
    } catch (const std::runtime_error& e) {
        ui.displayError("Failed to get API key for fetching models: " + std::string(e.what()));
        return "";
    }

    struct curl_slist* headers = nullptr;
    auto headers_guard = std::unique_ptr<struct curl_slist, decltype(&curl_slist_free_all)>{nullptr, curl_slist_free_all};

    headers = curl_slist_append(headers, ("Authorization: Bearer " + api_key).c_str());
    headers = curl_slist_append(headers, "HTTP-Referer: https://llm-cli.tsatsin.com"); // As per plan's example headers
    headers = curl_slist_append(headers, "X-Title: LLM-cli"); // As per plan's example headers
    // Content-Type is not typically needed for a GET request without a body.
    headers_guard.reset(headers);

    std::string response_buffer;
    const char* api_url = OPENROUTER_API_URL_MODELS;

    curl_easy_setopt(curl, CURLOPT_URL, api_url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    // GET is the default, but can be explicit: curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback); // From curl_utils.h
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_buffer);
    // It's good practice to set a timeout
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L); // 30 seconds timeout

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        ui.displayError("API request to fetch models failed: " + std::string(curl_easy_strerror(res)));
        return "";
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code != 200) {
        ui.displayError("API request to fetch models returned HTTP status " + std::to_string(http_code) + ". Response: " + response_buffer);
        return ""; // Return empty on non-200 status
    }

    return response_buffer;
}
std::vector<ModelData> ChatClient::parseModelsFromAPIResponse(const std::string& api_response) {
    std::vector<ModelData> parsed_models;
    if (api_response.empty()) {
        ui.displayError("API response string is empty, cannot parse models.");
        return parsed_models;
    }

    try {
        nlohmann::json j = nlohmann::json::parse(api_response);

        if (j.contains("data") && j["data"].is_array()) {
            for (const auto& model_obj : j["data"]) {
                ModelData model_item;
                model_item.id = model_obj.value("id", "");
                // Fallback name to id if 'name' is not present or empty
                model_item.name = model_obj.value("name", "");
                if (model_item.name.empty()) {
                    model_item.name = model_item.id;
                }
                
                // Example of extracting other fields (add as needed based on ModelData struct)
                // model_item.context_length = model_obj.value("context_length", 0);
                // if (model_obj.contains("pricing") && model_obj["pricing"].is_object()) {
                //     model_item.input_cost_per_mtok = std::stod(model_obj["pricing"].value("input", "0.0"));
                //     model_item.output_cost_per_mtok = std::stod(model_obj["pricing"].value("output", "0.0"));
                // }


                if (!model_item.id.empty()) {
                    parsed_models.push_back(model_item);
                }
            }
        } else {
            ui.displayError("Failed to parse models: 'data' field not found or not an array in API response. Response: " + api_response.substr(0, 500));
        }
    } catch (const nlohmann::json::parse_error& e) {
// Handle the JSON parsing error
        ui.displayError("JSON parsing error in parseModelsFromAPIResponse: " + std::string(e.what()));
        // parseModelsFromAPIResponse should return std::vector<ModelData>
        return {}; // Return an empty vector in case of error
    } // Closing brace for the catch block

    // Assuming 'parsed_models' is the variable holding the successfully parsed models
    // from the try block of parseModelsFromAPIResponse. This is the normal return path.
    return parsed_models;
} // Closing brace for the parseModelsFromAPIResponse function
void ChatClient::cacheModelsToDB(const std::vector<ModelData>& models) {
    if (models.empty()) {
        ui.displayStatus("No models to cache.");
        return;
    }
    try {
        ui.displayStatus("Clearing existing models table...");
        db.clearModelsTable(); // This method needs to be implemented in PersistenceManager

        ui.displayStatus("Caching " + std::to_string(models.size()) + " models to database...");
        for (const ModelData& model_item : models) {
            if (model_item.id.empty()) {
                ui.displayError("Skipping model with empty ID during caching.");
                continue;
            }
            db.insertOrUpdateModel(model_item); // This method needs to be implemented in PersistenceManager
        }
        ui.displayStatus("Finished caching models to DB.");
    } catch (const std::exception& e) {
        ui.displayError("Error during model caching: " + std::string(e.what()));
        // Depending on the error, we might want to indicate that the cache could be incomplete.
    }
}
        ui.displayError("Failed to parse models API response JSON: " + std::string(e.what()) + ". Response snippet: " + api_response.substr(0, 500));
    } catch (const std::exception& e) {
        ui.displayError("An unexpected error occurred during model parsing: " + std::string(e.what()));
    }
    return parsed_models;
}
// --- ChatClient Method Implementations ---

std::string ChatClient::makeApiCall(const std::vector<Message>& context, bool use_tools) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }
    // RAII wrapper for CURL handle
    auto curl_guard = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>{curl, curl_easy_cleanup};


    std::string api_key = get_openrouter_api_key(); // Now safe - no leaks on throw

    struct curl_slist* headers = nullptr;
    // Use RAII for headers too
    auto headers_guard = std::unique_ptr<struct curl_slist, decltype(&curl_slist_free_all)>{nullptr, curl_slist_free_all};

    headers = curl_slist_append(headers, ("Authorization: Bearer " + api_key).c_str());
    headers_guard.reset(headers); // Transfer ownership to guard
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "HTTP-Referer: https://llm-cli.tsatsin.com");
    headers = curl_slist_append(headers, "X-Title: LLM-cli");

    nlohmann::json payload;
    payload["model"] = this->model_name; // Use member variable
    payload["messages"] = nlohmann::json::array();

    // --- Secure Conversation History Construction ---
    // This logic ensures that only valid sequences of assistant tool_calls and their
    // corresponding tool result messages are included in the payload sent to the API.
    // An assistant message requesting tool calls is only included if ALL required
    // tool result messages appear later in the history. Otherwise, the incomplete
    // assistant message and its subsequent (now orphaned) tool results are discarded.
    std::unordered_set<std::string> valid_tool_ids; // Stores IDs of tool calls whose results should be included.
    nlohmann::json msg_array = nlohmann::json::array(); // The reconstructed message array for the payload.

    for (size_t i = 0; i < context.size(); ++i) {
        const Message& msg = context[i];

        /* ---- 1. Process Assistant Messages Potentially Containing Tool Calls ---- */
        // Check if the message is from the assistant and its content might be a JSON object (starts with '{').
        if (msg.role == "assistant" && !msg.content.empty() && msg.content.front() == '{') {
            try {
                auto asst_json = nlohmann::json::parse(msg.content);
                // Check if the parsed JSON contains the "tool_calls" key.
                if (asst_json.contains("tool_calls")) {
                    // Collect all tool_call IDs requested by this assistant message.
                    std::vector<std::string> ids;
                    for (const auto& tc : asst_json["tool_calls"]) {
                        if (tc.contains("id")) ids.push_back(tc["id"].get<std::string>());
                    }

                    // Verify that *every* requested tool_call ID has a corresponding 'tool' message later in the history.
                    bool all_results_present = true;
                    for (const auto& id : ids) {
                        bool found_result = false;
                        // Search forward from the current assistant message.
                        for (size_t j = i + 1; j < context.size() && !found_result; ++j) {
                            if (context[j].role != "tool") continue; // Skip non-tool messages.
                            try {
                                // Parse the 'tool' message content to find its tool_call_id.
                                auto tool_json = nlohmann::json::parse(context[j].content);
                                found_result = tool_json.contains("tool_call_id") &&
                                               tool_json["tool_call_id"] == id;
                            } catch (...) { /* Ignore malformed tool messages */ }
                        }
                        if (!found_result) {
                            all_results_present = false; // If any result is missing, mark this assistant message as invalid.
                            break;
                        }
                    }

                    // If not all results were found, discard this assistant message and its potential tool calls.
                    if (!all_results_present) continue; // Skip to the next message in history.

                    // If all results are present, include this assistant message (tool_calls only, content is null)
                    // and mark its tool_call IDs as valid for inclusion later.
                    msg_array.push_back({{"role", "assistant"},
                                         {"content", nullptr}, // Content is null when tool_calls are present
                                         {"tool_calls", asst_json["tool_calls"]}});
                    valid_tool_ids.insert(ids.begin(), ids.end()); // Add these IDs to the set of valid ones.
                    continue; // Skip the generic message handling below.
                }
            } catch (...) { /* If content is not valid JSON, treat as a normal message below. */ }
        }

        /* ---- 2. Process Tool Messages ---- */
        if (msg.role == "tool") {
            try {
                auto tool_json = nlohmann::json::parse(msg.content);
                // Include this tool message *only if* its tool_call_id corresponds to a previously validated assistant message.
                if (tool_json.contains("tool_call_id") &&
                    valid_tool_ids.count(tool_json["tool_call_id"].get<std::string>())) {
                    // Reconstruct the tool message in the required API format.
                    msg_array.push_back({{"role", "tool"},
                                         {"tool_call_id", tool_json["tool_call_id"]},
                                         {"name", tool_json["name"]}, // Assuming 'name' is present in stored JSON
                                         {"content", tool_json["content"]}}); // Assuming 'content' is present
                }
            } catch (...) { /* Ignore malformed or invalid tool messages. */ }
            continue; // Always skip generic handling for 'tool' role messages.
        }

        /* ---- 3. Process Normal User/Assistant Messages ---- */
        // Include standard user messages and assistant messages that didn't contain valid tool_calls.
        msg_array.push_back({{"role", msg.role}, {"content", msg.content}});
    }
    // --- End Secure Conversation History Construction ---

    payload["messages"] = std::move(msg_array); // Use the securely constructed message array.

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
    // curl_slist_free_all(headers); // No longer needed, handled by headers_guard (RAII)
    // curl_easy_cleanup(curl); // No longer needed, handled by curl_guard (RAII)

    if (res != CURLE_OK) {
        // The unique_ptrs (curl_guard, headers_guard) automatically clean up CURL handle and headers list on error/exit.
        throw std::runtime_error("API request failed: " + std::string(curl_easy_strerror(res)));
    }

    // Return the full response string, not just the content, 
    // as we need to check for tool_calls later.
    return response; 
} // <-- ADDED MISSING CLOSING BRACE FOR makeApiCall

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
        // Pass the ui instance for status messages
        tool_result_str = toolManager.execute_tool(db, *this, ui, function_name, function_args);
    } catch (const std::exception& e) {
         // Errors during argument validation or unknown tool are caught here
         ui.displayError("Tool execution error for '" + function_name + "': " + e.what()); // Use UI for error
         tool_result_str = "Error executing tool '" + function_name + "': " + e.what();
         // Continue to prepare the error as the tool result
    }
    // Note: User feedback like "[Searching web...]" is now handled within toolMgr.execute_tool()

    // Prepare tool result message content as JSON string
    nlohmann::json tool_result_content;
    tool_result_content["role"] = "tool"; // Explicitly add role
    tool_result_content["tool_call_id"] = tool_call_id;
    tool_result_content["name"] = function_name;
    tool_result_content["content"] = tool_result_str; // Contains result or error message

    // Return the JSON string representation of the tool result message
    return tool_result_content.dump();
}


// Main application loop
void ChatClient::run() { // Removed std::stop_token
    db.cleanupOrphanedToolMessages();
    std::string initial_message = "Chatting with " + this->model_name + " - Type your message";
    if (!ui.isGuiMode()) {
        initial_message += " (Ctrl+D to exit)";
    }
    initial_message += "\n";
    ui.displayOutput(initial_message); // Use UI, message adapted for CLI/GUI
    while (true) {
        // Stop request check removed
        try {
            auto input_opt = promptUserInput();
            if (!input_opt) break;          // Exit loop if UI signals shutdown (e.g., Ctrl+D or window close)
            if (input_opt->empty()) continue; // Ignore empty input lines
            processTurn(*input_opt);        // Process the user's input for this turn
        } catch (const std::exception& e) {
            // Catch potential exceptions escaping processTurn or from promptUserInput itself
            ui.displayError("Unhandled error in main loop: " + std::string(e.what()));
            // Optionally add a small delay or other recovery mechanism here
        } catch (...) {
            // Catch any non-standard exceptions
            ui.displayError("An unknown, non-standard error occurred in the main loop.");
        }
    }
}

std::optional<std::string> ChatClient::promptUserInput() {
    // Delegate input prompting to the injected UI object
    return ui.promptUserInput();
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
        ui.displayError("API Error Received: " + api_response["error"].dump(2)); // Use UI for error
        // Check for the specific recoverable error
        if (api_response["error"].contains("code") &&
            api_response["error"]["code"] == "tool_use_failed" &&
            api_response["error"].contains("failed_generation") &&
            api_response["error"]["failed_generation"].is_string())
        {
            fallback_content = api_response["error"]["failed_generation"];
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
        // Fall through to the fallback parsing logic below
    } else {
         // Unexpected response structure
         ui.displayError("Invalid API response structure (First Response). Response was: " + api_response.dump()); // Use UI for error
         return true;
    }
    return false;
}

/* ======== Standard `tool_calls` Execution Path ======== */
// This function handles the scenario where the API response contains standard `tool_calls`.
// It executes the requested tools, saves the results, makes a final API call
// to get the text response based on the tool results, and handles potential errors/retries.
// Returns true if the entire path (including the final text response) was successful.
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

    // Helper lambda to build and store argument error messages
    auto buildArgError = [&](const std::string& tool_call_id, const std::string& function_name, const std::string& error_msg) {
        nlohmann::json err;
        err["tool_call_id"] = tool_call_id;
        err["name"]         = function_name;
        err["content"]      = error_msg;
        tool_result_messages.push_back(err.dump());
        any_tool_executed = true; // Mark that we attempted execution even if args failed
    };

    for (const auto& tool_call : response_message["tool_calls"]) {
        if (!tool_call.contains("id") || !tool_call.contains("function") || !tool_call["function"].contains("name") || !tool_call["function"].contains("arguments")) {
            // Malformed tool_call object received. Skipping.
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
            buildArgError(tool_call_id, function_name, "Error: Failed to parse arguments JSON: " + std::string(e.what()));
            continue; // Skip normal execution for this tool call
        } catch (const nlohmann::json::type_error& e) {
            // Handle cases where arguments might not be a string initially
            buildArgError(tool_call_id, function_name, "Error: Invalid argument type: " + std::string(e.what()));
            continue;
        }

        // Call the helper function to execute the tool and get the result message JSON string
        std::string result_msg_json = executeAndPrepareToolResult(tool_call_id, function_name, function_args);
        tool_result_messages.push_back(result_msg_json);
        any_tool_executed = true;
    }

    // If no tools were actually executed (e.g., all malformed), return false
    if (!any_tool_executed) {
        // Assistant requested tool calls, but none could be executed.
        return false;
    }

    // 3. Save all collected tool results to the database within a transaction
    try {
        db.beginTransaction(); // Start transaction
        for (const auto& msg_json : tool_result_messages) {
            db.saveToolMessage(msg_json);
        }
        db.commitTransaction(); // Commit transaction
    } catch (const std::exception& e) {
        db.rollbackTransaction(); // Rollback on error
        ui.displayError("Database error saving tool results: " + std::string(e.what())); // Use UI for error
        // Error saving tool results to database (handled by returning false)
        return false; // Indicate failure if DB save fails
    }


    // 4. Reload context INCLUDING the assistant message and ALL tool results
    context = db.getContextHistory();

    // 5. Make the final API call to get the text response
    std::string final_content;
    bool final_response_success = false;
    std::string final_response_str; // Declare outside the loop

    // Retry loop (up to 3 attempts) to get the final text response after tool execution.
    // This handles cases where the API might initially fail or unexpectedly return tool calls again.
    for (int attempt = 0; attempt < 3 && !final_response_success; attempt++) {
        // On the 2nd and 3rd attempts, add a temporary system message to strongly discourage further tool use.
        if (attempt > 0) {
            Message no_tool_msg{"system", "IMPORTANT: Do not use any tools or functions in your response. Provide a direct text answer only."};
            context.push_back(no_tool_msg); // Add temporary instruction
            final_response_str = makeApiCall(context, /*use_tools=*/false); // Tools explicitly disabled
            context.pop_back(); // Remove the temporary instruction before next iteration or saving
        } else {
            // First attempt: Call API with the updated context (including tool results), tools disabled.
            final_response_str = makeApiCall(context, /*use_tools=*/false);
        }

        nlohmann::json final_response_json;
        try {
            final_response_json = nlohmann::json::parse(final_response_str);
        } catch (const nlohmann::json::parse_error& e) {
            // JSON Parsing Error (Final Response) - handled by retry/failure
            if (attempt == 2) break; // Exit loop on last attempt
            continue; // Try again
        }

        // Check for API errors in the final response
        if (final_response_json.contains("error")) {
             ui.displayError("API Error Received (Final Response): " + final_response_json["error"].dump(2)); // Use UI for error
             if (attempt == 2) break;
             continue; // Try again
        }

        // Check if we have a valid response structure
        if (!final_response_json.contains("choices") || final_response_json["choices"].empty() ||
            !final_response_json["choices"][0].contains("message")) {
            // Invalid API response structure (Final Response) - handled by retry/failure
            if (attempt == 2) break;
            continue; // Try again
        }

        auto& message = final_response_json["choices"][0]["message"];

        // Check if the final message *still* contains tool_calls (should be rare now)
        if (message.contains("tool_calls") && !message["tool_calls"].is_null()) {
            // Warning: Final response unexpectedly contains tool_calls. Retrying with explicit instruction.
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
            // Final response message missing content field - handled by retry/failure
            if (attempt == 2) break;
            continue; // Try again
        }
    }

    // 6. Handle the final response
    if (!final_response_success) {
        ui.displayError("Failed to get a valid final text response after tool execution and 3 attempts."); // Use UI for error
        return false; // Indicate overall failure for this turn's tool path
    }

    // Save the final assistant response
    db.saveAssistantMessage(final_content);

    // Display final response
    ui.displayOutput(final_content + "\n\n"); // Use UI for output
    return true; // Indicate success for the standard tool call path
}

/* ======== Fallback `<function>` Tag Parsing and Execution ======== */
// This function handles the scenario where the API response doesn't use standard `tool_calls`
// but instead embeds function calls within XML-like `<function>` tags in the content string.
// It parses these tags, executes the functions, saves results, makes a final API call,
// and handles errors/retries, similar to the standard path.
// Returns true if at least one fallback function was successfully executed and led to a final response.
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

        // --- Argument Parsing Logic ---
        // Attempt to parse function name and arguments based on delimiters like '(', '{', or ','.
        // This is complex due to the unstructured nature of fallback tags.

        // Find the first potential argument delimiter '(', '{', or ',' after the function name starts.
        size_t args_delimiter_start = content_str.find_first_of("{(,", name_start);

        // Ignore delimiter if it's after the closing tag.
        if (args_delimiter_start != std::string::npos && args_delimiter_start >= func_end) {
            args_delimiter_start = std::string::npos;
        }

        std::string function_name;
        nlohmann::json function_args = nlohmann::json::object(); // Default to empty object
        bool parsed_args_or_no_args_needed = false; // Flag indicating if args were parsed or if none were expected

        // Case 1: Delimiter found ('{', '(', or ',')
        if (args_delimiter_start != std::string::npos) {
            // Extract function name up to the delimiter.
            function_name = content_str.substr(name_start, args_delimiter_start - name_start);
            char open_delim = content_str[args_delimiter_start];

            // Subcase 1a: Arguments enclosed in '{...}' or '(...)'
            if (open_delim == '{' || open_delim == '(') {
                char close_delim = (open_delim == '{') ? '}' : ')';
                // Find the *last* matching closing delimiter before the </function> tag.
                size_t search_end_pos = func_end - 1;
                size_t args_end = content_str.rfind(close_delim, search_end_pos);

                if (args_end != std::string::npos && args_end > args_delimiter_start) {
                    // Extract the argument string (including delimiters).
                    std::string args_str = content_str.substr(args_delimiter_start, args_end - args_delimiter_start + 1);
                    try {
                        // Trim whitespace and potentially outer parentheses before parsing as JSON.
                        std::string trimmed_args = args_str;
                        trimmed_args.erase(0, trimmed_args.find_first_not_of(" \n\r\t"));
                        trimmed_args.erase(trimmed_args.find_last_not_of(" \n\r\t") + 1);
                        // Handle cases like <function(search_web({"query": "..."}))>
                        if (trimmed_args.size() >= 2 && trimmed_args.front() == '(' && trimmed_args.back() == ')') {
                            trimmed_args = trimmed_args.substr(1, trimmed_args.size() - 2);
                            trimmed_args.erase(0, trimmed_args.find_first_not_of(" \n\r\t"));
                            trimmed_args.erase(trimmed_args.find_last_not_of(" \n\r\t") + 1);
                        }
                        function_args = nlohmann::json::parse(trimmed_args);
                        parsed_args_or_no_args_needed = true;
                    } catch (const nlohmann::json::parse_error& e) {
                        // ui.displayWarning("Failed to parse arguments JSON from <function...>. Treating as empty args.");
                        function_args = nlohmann::json::object(); // Fallback to empty args on parse error
                        parsed_args_or_no_args_needed = true;
                    }
                } else {
                    // ui.displayWarning("Malformed arguments: Found open delimiter but no matching close delimiter before </function>.");
                    // Keep empty args, proceed as if args might not be needed.
                     parsed_args_or_no_args_needed = true;
                }
            // Subcase 1b: Arguments potentially after a comma (less common/reliable)
            } else if (open_delim == ',') {
                size_t args_start_pos = args_delimiter_start + 1;
                size_t args_end_pos = func_end; // Assume args go up to the end tag
                if (args_end_pos > args_start_pos) {
                    std::string args_str = content_str.substr(args_start_pos, args_end_pos - args_start_pos);
                    try {
                        function_args = nlohmann::json::parse(args_str);
                        parsed_args_or_no_args_needed = true;
                    } catch (const nlohmann::json::parse_error& e) {
                        // ui.displayWarning("Failed to parse arguments JSON after comma in <function...>. Treating as empty args.");
                        function_args = nlohmann::json::object();
                        parsed_args_or_no_args_needed = true;
                    }
                } else {
                    // ui.displayWarning("Found comma delimiter but no arguments before </function>.");
                    function_args = nlohmann::json::object();
                    parsed_args_or_no_args_needed = true;
                }
            }
        // Case 2: No explicit delimiter '{', '(', or ',' found before </function>
        } else {
            // Check for a special case where the function name is immediately followed by '{' or '(',
            // e.g., <function(search_web={"query":...})</function> or <function{tool_name={"arg":...}}</function>
            size_t brace_pos = content_str.find_first_of("{(", name_start);
            if (brace_pos != std::string::npos && brace_pos < func_end) {
                // Extract name up to the brace/paren
                function_name = content_str.substr(name_start, brace_pos - name_start);
                char open_delim = content_str[brace_pos];
                char close_delim = (open_delim == '{') ? '}' : ')';
                size_t search_end_pos = func_end - 1;
                size_t args_end = content_str.rfind(close_delim, search_end_pos);

                if (args_end != std::string::npos && args_end > brace_pos) {
                    // Extract and parse the arguments within the braces/parens
                    std::string args_str = content_str.substr(brace_pos, args_end - brace_pos + 1);
                    try {
                        // Trim and parse (similar to Subcase 1a)
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
                        // ui.displayWarning("Failed to parse arguments JSON from <function...>. Treating as empty args.");
                        function_args = nlohmann::json::object();
                        parsed_args_or_no_args_needed = true;
                    }
                } else {
                    // ui.displayWarning("Malformed arguments: Found open delimiter but no matching close delimiter before </function>.");
                    parsed_args_or_no_args_needed = true; // Assume no args needed
                }
            } else {
                // Case 3: No delimiters found at all - assume function name takes the whole space.
                function_name = content_str.substr(name_start, func_end - name_start);
                parsed_args_or_no_args_needed = true; // Assume no arguments needed for this function
            }
        }
        // --- End Argument Parsing Logic ---

        // Clean up extracted function name (trim whitespace, remove trailing brackets)
        if (!function_name.empty()) {
            function_name.erase(0, function_name.find_first_not_of(" \n\r\t"));
            function_name.erase(function_name.find_last_not_of(" \n\r\t") + 1);
            // Remove trailing stray characters like '[', '(', '{' that might be left from parsing
            while (!function_name.empty() &&
                   (function_name.back() == '[' || function_name.back() == '(' || function_name.back() == '{')) {
                function_name.pop_back();
                // Also trim any whitespace revealed after popping
                while (!function_name.empty() && isspace(function_name.back())) {
                    function_name.pop_back();
                }
            }
        }

        // Specific fix for web_research tool sometimes using 'query' instead of 'topic' in fallback tags.
        if (function_name == "web_research" && function_args.contains("query") && !function_args.contains("topic")) {
            // ui.displayStatus("Adjusting 'query' to 'topic' for web_research fallback.");
            function_args["topic"] = function_args["query"];
            function_args.erase("query");
        }

        // Proceed if we have a function name and either parsed args or determined no args were needed.
        if (!function_name.empty() && parsed_args_or_no_args_needed) {
            // Recovery attempt: If args are still empty, try parsing the *first* {...} block within the tag content.
            if (function_args.empty()) {
                size_t brace_start = content_str.find('{', name_start);
                // Ensure brace_start is before the closing tag
                if (brace_start != std::string::npos && brace_start < func_end) {
                    size_t brace_end = content_str.find('}', brace_start);
                    // Ensure brace_end is also before the closing tag
                    if (brace_end != std::string::npos && brace_end < func_end) {
                        std::string possible_args = content_str.substr(brace_start, brace_end - brace_start + 1);
                        try {
                            nlohmann::json recovered_args = nlohmann::json::parse(possible_args);
                            function_args = recovered_args; // Use recovered args if parsing succeeds
                        } catch (...) {
                            // Ignore parse errors, keep empty args if recovery fails
                        }
                    }
                }
            }

            // Save the original assistant message containing the <function> tag.
            std::string function_block = content_str.substr(func_start, func_end + 11 - func_start); // Include </function>
            db.saveAssistantMessage(function_block);
            context = db.getContextHistory(); // Reload context including the saved message

            // Generate a synthetic tool_call_id for this fallback execution.
            std::string tool_call_id = "synth_" + std::to_string(++synthetic_tool_call_counter);

            // Execute the tool using the parsed name and arguments.
            // Status updates are handled within executeAndPrepareToolResult -> toolManager.execute_tool.
            std::string tool_result_msg_json = executeAndPrepareToolResult(tool_call_id, function_name, function_args);

            // Save the tool result message (JSON string).
            try {
                db.beginTransaction();
                db.saveToolMessage(tool_result_msg_json);
                db.commitTransaction();
            } catch (const std::exception& e) {
                db.rollbackTransaction();
                ui.displayError("Database error saving fallback tool result: " + std::string(e.what()));
                search_pos = func_end + 11; // Move past this failed tag
                continue; // Skip API call for this failed save
            }

            // Reload context again, now including the tool result.
            context = db.getContextHistory();

            // Make the final API call to get the text response based on the fallback tool execution.
            // Retry logic is similar to the standard tool call path.
            std::string final_content;
            bool final_response_success = false;
            std::string final_response_str;
            for (int attempt = 0; attempt < 3 && !final_response_success; attempt++) {
                 // Add temporary system message on retries to prevent further tool use.
                 if (attempt > 0) {
                     Message no_tool_msg{"system", "IMPORTANT: Do not use any tools or functions in your response. Provide a direct text answer only."};
                     context.push_back(no_tool_msg);
                     final_response_str = makeApiCall(context, /*use_tools=*/false);
                     context.pop_back();
                 } else {
                     final_response_str = makeApiCall(context, /*use_tools=*/false);
                 }

                 // Parse and validate the final response.
                 nlohmann::json final_response_json;
                 try {
                     final_response_json = nlohmann::json::parse(final_response_str);
                 } catch (const nlohmann::json::parse_error& e) {
                     if (attempt == 2) { ui.displayError("Failed to parse final API response after fallback tool."); break; }
                     continue; // Retry on parse error
                 }
                 if (final_response_json.contains("error")) {
                      ui.displayError("API Error (Fallback Final Response): " + final_response_json["error"].dump(2));
                      if (attempt == 2) break;
                      continue; // Retry on API error
                 }
                 if (!final_response_json.contains("choices") || final_response_json["choices"].empty() || !final_response_json["choices"][0].contains("message")) {
                     if (attempt == 2) { ui.displayError("Invalid final API response structure after fallback tool."); break; }
                     continue; // Retry on invalid structure
                 }
                 auto& message = final_response_json["choices"][0]["message"];
                 // Check again for unexpected tool calls in the final response.
                 if (message.contains("tool_calls") && !message["tool_calls"].is_null()) {
                     if (attempt == 2) { ui.displayError("Final response still contained tool_calls after fallback execution."); break; }
                     continue; // Retry if tools were unexpectedly called again
                 }
                 // Extract the final content if present.
                 if (message.contains("content") && message["content"].is_string()) {
                     final_content = message["content"];
                     final_response_success = true;
                     break; // Success!
                 } else {
                     if (attempt == 2) { ui.displayError("Final response message missing content after fallback tool."); break; }
                     continue; // Retry if content is missing
                 }
            }

            // If we successfully got a final text response:
            if (final_response_success) {
                db.saveAssistantMessage(final_content); // Save the final assistant message
                ui.displayOutput(final_content + "\n\n"); // Display it
                any_executed = true; // Mark that this fallback path was successful for at least one tag
            } else {
                 ui.displayError("Failed to get final response after fallback tool execution for: " + function_name);
                 // Continue searching for other tags even if this one failed its final step.
            }
        } else {
             // If function name was empty or args couldn't be parsed properly, skip execution for this tag.
             // ui.displayWarning("Skipping malformed or unparsable <function> tag.");
        }

        // Move search position past the current </function> tag to find the next one.
        search_pos = func_end + 11; // Length of "</function>" is 11
    }

    return any_executed; // Return true if at least one fallback function was fully processed.
}

/* ======== Display and Save Final Assistant Content ======== */
// Handles the case where the API response is a direct text message (no tool calls).
// Saves the message to the database and displays it via the UI.
void ChatClient::printAndSaveAssistantContent(const nlohmann::json& response_message)
{
    if (!response_message.is_null() && response_message.contains("content")) {
        if (response_message["content"].is_string()) {
            std::string txt = response_message["content"];
            db.saveAssistantMessage(txt);
            ui.displayOutput(txt + "\n\n"); // Use UI for output
        } else if (!response_message["content"].is_null()) {
            std::string dumped = response_message["content"].dump();
            db.saveAssistantMessage(dumped);
            ui.displayOutput(dumped + "\n\n"); // Use UI for output
        }
    }
}

// Processes a single turn of the conversation.
void ChatClient::processTurn(const std::string& input) {
    try {
        // 1. Save the user's input message.
        saveUserInput(input);
        // 2. Load the current conversation history.
        auto context = db.getContextHistory();

        // 3. Make the initial API call, enabling tools.
        ui.displayStatus("Waiting for response..."); // Update status
        std::string api_raw_response = makeApiCall(context, /*use_tools=*/true);
        ui.displayStatus("Processing response..."); // Update status
        nlohmann::json api_response_json = nlohmann::json::parse(api_raw_response);

        // 4. Check for API errors and extract initial response message or fallback content.
        std::string fallback_content; // Used if standard tool calls fail or aren't present, but content exists.
        nlohmann::json response_message; // The 'message' object from the API response.
        if (handleApiError(api_response_json, fallback_content, response_message)) {
            ui.displayStatus("Ready."); // Reset status on error
            return; // Turn ends prematurely due to unrecoverable API error.
        }

        // 5. Attempt to execute standard tool calls if present in the response.
        //    This function handles the entire flow: save assistant msg -> execute tools -> save results -> final API call -> save/display final response.
        bool turn_completed_via_standard_tools = executeStandardToolCalls(response_message, context);

        // 6. If standard tools were NOT executed (or failed partway) AND we have fallback content (from initial response or error recovery):
        //    Attempt to parse and execute fallback <function> tags within that content.
        //    This function also handles the full flow for the fallback path.
        bool turn_completed_via_fallback_tools = false;
        if (!turn_completed_via_standard_tools && !fallback_content.empty()) {
            turn_completed_via_fallback_tools = executeFallbackFunctionTags(fallback_content, context);
        }

        // 7. If neither the standard tool path nor the fallback tool path completed the turn:
        //    Assume the initial response was a direct text message (or contained only unexecutable tools/tags).
        //    Print and save the content from the initial `response_message`.
        if (!turn_completed_via_standard_tools && !turn_completed_via_fallback_tools) {
            printAndSaveAssistantContent(response_message);
        }

        ui.displayStatus("Ready."); // Reset status after successful turn processing

    } catch (const nlohmann::json::parse_error& e) {
        // Catch JSON parsing errors occurring outside the specific tool call paths.
        ui.displayError("Error parsing API response: " + std::string(e.what()));
        ui.displayStatus("Error."); // Set error status
    } catch (const std::runtime_error& e) {
        // Catch CURL errors or other runtime errors from makeApiCall or DB operations.
        ui.displayError("Runtime error: " + std::string(e.what()));
         ui.displayStatus("Error."); // Set error status
    } catch (const std::exception& e) {
        // Catch any other standard exceptions.
        ui.displayError("An unexpected error occurred: " + std::string(e.what()));
         ui.displayStatus("Error."); // Set error status
    } catch (...) {
        // Catch any non-standard exceptions.
        ui.displayError("An unknown, non-standard error occurred.");
        ui.displayStatus("Error."); // Set error status
    }
}
