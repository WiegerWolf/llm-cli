#include <iostream>
#include <string>
#include <curl/curl.h>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <readline/readline.h>
#include <readline/history.h>
// Removed functional, gumbo.h, fstream, chrono, ctime, iomanip (moved to tools.cpp)
#include "database.h"
#include "tools.h" // Include the new ToolManager header

using namespace std;

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, string* output) {
    size_t total_size = size * nmemb;
    output->append((char*)contents, total_size);
    return total_size;
}

// Helper functions find_node_by_tag, find_node_by_tag_and_class moved to tools.cpp


class ChatClient {
private:
    PersistenceManager db;
    ToolManager toolManager; // Instantiate ToolManager
    string api_base = "https://api.groq.com/openai/v1/chat/completions";
    string model_name = "llama-3.3-70b-versatile"; // Added model name member
    // Removed tool JSON definitions (moved to ToolManager)

    // Tool implementation functions (gumbo_get_text, parse_ddg_html, visit_url, search_web)
    // have been moved to tools.cpp and are accessed via toolManager instance.

public: // Make makeApiCall public so ToolManager can use it for synthesis
    // Modified to optionally include tools (gets definitions from ToolManager)
    string makeApiCall(const vector<Message>& context, bool use_tools = false) {
        CURL* curl = curl_easy_init();
        if (!curl) {
            throw runtime_error("Failed to initialize CURL");
        }

        const char* api_key = getenv("GROQ_API_KEY");
        if (!api_key) {
            throw runtime_error("GROQ_API_KEY environment variable not set!");
        }

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, ("Authorization: Bearer " + string(api_key)).c_str());
        headers = curl_slist_append(headers, "Content-Type: application/json");

        nlohmann::json payload;
        payload["model"] = this->model_name; // Use member variable
        payload["messages"] = nlohmann::json::array();
        
        // Add conversation history
        for (const auto& msg : context) {
            // Handle different message structures (simple content vs. tool calls/responses)
            if (msg.role == "assistant" && !msg.content.empty() && msg.content.front() == '{') { 
                // Attempt to parse content as JSON for potential tool_calls
                try {
                    auto content_json = nlohmann::json::parse(msg.content);
                    if (content_json.contains("tool_calls")) {
                         payload["messages"].push_back({{"role", msg.role}, {"content", nullptr}, {"tool_calls", content_json["tool_calls"]}});
                         continue; // Skip adding simple content if tool_calls are present
                    }
                } catch (const nlohmann::json::parse_error& e) {
                    // Not valid JSON or doesn't contain tool_calls, treat as regular content
                }
            } else if (msg.role == "tool") {
                 try {
                    // The content is expected to be a JSON string like:
                    // {"tool_call_id": "...", "name": "...", "content": "..."}
                    auto content_json = nlohmann::json::parse(msg.content);
                    payload["messages"].push_back({
                        {"role", msg.role}, 
                        {"tool_call_id", content_json["tool_call_id"]}, 
                        {"name", content_json["name"]},
                        {"content", content_json["content"]} // The actual tool result string
                    });
                    continue; // Skip default content handling
                 } catch (const nlohmann::json::parse_error& e) {
                     // Handle potential error or malformed tool message content
                     std::cerr << "Warning: Could not parse tool message content for ID " << msg.id << std::endl;
                 }
            }
            // Default handling for user messages and simple assistant messages
            payload["messages"].push_back({{"role", msg.role}, {"content", msg.content}});
        }

        // Add tools if requested
        if (use_tools) {
            // Get tool definitions from the ToolManager
            payload["tools"] = toolManager.get_tool_definitions(); 
            payload["tool_choice"] = "auto";
        }

        string json_payload = payload.dump();
        // std::cout << "DEBUG: Payload: " << json_payload << std::endl; // Uncomment for debugging API payload
        string response;

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
            throw runtime_error("API request failed: " + string(curl_easy_strerror(res)));
        }

        // Return the full response string, not just the content, 
        // as we need to check for tool_calls later.
        return response; 
    }

private: 
    // Removed tool implementations (get_current_datetime, read_history) - moved to ToolManager
    // Removed tool helpers


    // Helper function now takes ToolManager reference and uses it to execute tools
    bool handleToolExecutionAndFinalResponse(
        ToolManager& toolMgr, // Pass ToolManager by reference
        const std::string& tool_call_id,
        const std::string& function_name,
        const nlohmann::json& function_args,
        std::vector<Message>& context // Pass context by reference to update it
    ) {
        string tool_result_str; 
        try {
            // Execute the tool using the ToolManager
            // Pass the db instance needed by some tools like read_history
            // Pass the ChatClient instance (*this) for tools that need to make API calls (like web_research)
            tool_result_str = toolMgr.execute_tool(db, *this, function_name, function_args);
        } catch (const std::exception& e) {
             // Errors during argument validation or unknown tool are caught here
             cerr << "Tool execution error for '" << function_name << "': " << e.what() << "\n";
             tool_result_str = "Error executing tool '" + function_name + "': " + e.what();
             // Continue to save this error as the tool result
        }
        // Note: User feedback like "[Searching web...]" is now handled within toolMgr.execute_tool()

        // Prepare tool result message content as JSON string
        nlohmann::json tool_result_content;
        tool_result_content["tool_call_id"] = tool_call_id;
        tool_result_content["name"] = function_name;
        tool_result_content["content"] = tool_result_str; // Contains result or error message

        // Save the tool's response message using the dedicated function
        db.saveToolMessage(tool_result_content.dump());

        // Reload context INCLUDING the tool result
        context = db.getContextHistory(); // Reload context

        // Make the second API call (tools are never needed for the final response)
        string final_response_str = makeApiCall(context, false);
        nlohmann::json final_response_json;
         try {
            final_response_json = nlohmann::json::parse(final_response_str);
        } catch (const nlohmann::json::parse_error& e) {
            cerr << "JSON Parsing Error (Second Response): " << e.what() << "\nResponse was: " << final_response_str << "\n";
            return false; // Indicate failure
        }

        if (!final_response_json.contains("choices") || final_response_json["choices"].empty() || !final_response_json["choices"][0].contains("message") || !final_response_json["choices"][0]["message"].contains("content")) {
            cerr << "Error: Invalid API response structure (Second Response).\nResponse was: " << final_response_str << "\n";
            return false; // Indicate failure
        }
        string final_content = final_response_json["choices"][0]["message"]["content"];

        // Save the final assistant response
        db.saveAssistantMessage(final_content);

        // Display final response
        cout << final_content << "\n\n";
        cout.flush(); // Ensure output is displayed before next prompt
        return true; // Indicate success
    }


public:
    void run() {
        cout << "Chatting with " << this->model_name << " - Type your message (Ctrl+D to exit)\n"; // Updated greeting
        static int synthetic_tool_call_counter = 0; // Counter for synthetic IDs
        
        while (true) {
            // Read input with readline for better line editing
            char* input_cstr = readline("> ");
            if (!input_cstr) break; // Exit on Ctrl+D
            
            string input(input_cstr);
            free(input_cstr);
            
            if (input.empty()) continue;

            // Removed standalone /search command block - use the tool via LLM instead

            try {
                // Persist user message
                db.saveUserMessage(input);
                
                // Get context for the first API call
                auto context = db.getContextHistory(); // Use default max_messages
                
                // Make the first API call, allowing the model to use tools
                string first_api_response_str = makeApiCall(context, true); // Enable tools
                nlohmann::json first_api_response;
                try {
                    first_api_response = nlohmann::json::parse(first_api_response_str);
                } catch (const nlohmann::json::parse_error& e) {
                    cerr << "JSON Parsing Error (First Response): " << e.what() << "\nResponse was: " << first_api_response_str << "\n";
                    continue; // Skip processing this turn
                }

                // Extract the message part of the response
                if (!first_api_response.contains("choices") || first_api_response["choices"].empty() || !first_api_response["choices"][0].contains("message")) {
                    cerr << "Error: Invalid API response structure (First Response).\nResponse was: " << first_api_response_str << "\n";
                    continue;
                }
                nlohmann::json response_message = first_api_response["choices"][0]["message"];
                
                bool tool_call_flow_completed = false; // Flag to track if tool flow (path 1 or 2) happened

                // --- Path 1: Standard Tool Calls ---
                if (response_message.contains("tool_calls") && !response_message["tool_calls"].is_null()) {
                    // Save the assistant's message requesting tool use
                    db.saveAssistantMessage(response_message.dump()); 
                    context = db.getContextHistory(); // Reload context including the tool call message

                    // Execute tools and get final response via helper
                    for (const auto& tool_call : response_message["tool_calls"]) {
                        if (!tool_call.contains("id") || !tool_call.contains("function") || !tool_call["function"].contains("name") || !tool_call["function"].contains("arguments")) {
                             cerr << "Error: Malformed tool_call object received.\n";
                             continue; // Skip this tool call
                        }
                        string tool_call_id = tool_call["id"];
                        string function_name = tool_call["function"]["name"];
                        nlohmann::json function_args;
                        try {
                             // First get the arguments as a string, then parse that string
                             std::string args_str = tool_call["function"]["arguments"].get<std::string>();
                             function_args = nlohmann::json::parse(args_str);
                        } catch (const nlohmann::json::parse_error& e) {
                             // If parsing fails, args_str might not have been initialized yet if get<> failed.
                             // Let's log the original JSON value instead for better debugging.
                             cerr << "JSON Parsing Error (Tool Arguments): " << e.what() << "\nArgs JSON was: " << tool_call["function"]["arguments"].dump() << "\n";
                             continue; // Skip this tool call
                        } catch (const nlohmann::json::type_error& e) {
                             // Handle cases where arguments might not be a string initially
                             cerr << "JSON Type Error (Tool Arguments): " << e.what() << "\nArgs JSON was: " << tool_call["function"]["arguments"].dump() << "\n";
                             continue; // Skip this tool call
                        }

                        // Call the helper function, passing the toolManager instance
                        if (handleToolExecutionAndFinalResponse(toolManager, tool_call_id, function_name, function_args, context)) {
                             tool_call_flow_completed = true; // Mark success for at least one tool
                        } else {
                             // Error was already printed by the helper or execute_tool.
                             // Decide if we should stop processing further tool calls in this turn? For now, continue.
                        }
                        // Context is updated within the helper after saving tool result
                    }
                // --- Path 2: Regular Response (No tool calls) ---
                } else if (response_message.contains("content") && response_message["content"].is_string()) {
                    // This path is taken if the response message does NOT contain 'tool_calls'
                    std::string content_str = response_message["content"];
                    db.saveAssistantMessage(content_str);
                    cout << content_str << "\n\n";
                    cout.flush();
                    tool_call_flow_completed = false; // Explicitly mark that no tool flow happened

                // --- Path 3: Handle Error Cases (No tool calls, content is null or not string) ---
                } else if (!tool_call_flow_completed) { // Only enter if tool_calls didn't happen
                    if (response_message.contains("content") && !response_message["content"].is_null()) {
                         // Content exists but isn't a string (unlikely but handled)
                         std::string non_string_content = response_message["content"].dump();
                         db.saveAssistantMessage(non_string_content);
                         cout << non_string_content << "\n\n";
                         cout.flush();
                    } else {
                         // No tool_calls and content is null or missing
                         cerr << "Error: API response missing content and tool_calls.\nResponse was: " << first_api_response_str << "\n";
                         // No message to save or print
                    }
                }
                // If tool_call_flow_completed is true, the helper function handled saving and printing the final response.
                // If it's false, the regular response (or error) was handled above.

            } catch (const nlohmann::json::parse_error& e) {
                 cerr << "JSON Parsing Error (Outer Loop): " << e.what() << "\n";
            } catch (const exception& e) {
                cerr << "Error: " << e.what() << "\n";
            }
        }
    }
}; 


int main() {
    ChatClient client;
    client.run();
    cout << "\nExiting...\n";
    return 0;
}
