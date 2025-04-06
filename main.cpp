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
#include <functional>
#include <gumbo.h>
#include <fstream>
#include "database.h"

using namespace std;

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, string* output) {
    size_t total_size = size * nmemb;
    output->append((char*)contents, total_size);
    return total_size;
}

// Helper function to recursively find the first node with a specific tag
static GumboNode* find_node_by_tag(GumboNode* node, GumboTag tag) {
    if (!node || node->type != GUMBO_NODE_ELEMENT) {
        return nullptr;
    }

    if (node->v.element.tag == tag) {
        return node;
    }

    GumboVector* children = &node->v.element.children;
    for (unsigned int i = 0; i < children->length; ++i) {
        GumboNode* found = find_node_by_tag(static_cast<GumboNode*>(children->data[i]), tag);
        if (found) {
            return found;
        }
    }
    return nullptr;
}

// Helper function to recursively find the first node with a specific tag and class
static GumboNode* find_node_by_tag_and_class(GumboNode* node, GumboTag tag, const std::string& class_name) {
     if (!node || node->type != GUMBO_NODE_ELEMENT) {
        return nullptr;
    }
    
    if (node->v.element.tag == tag) {
        GumboAttribute* class_attr = gumbo_get_attribute(&node->v.element.attributes, "class");
        if (class_attr && std::string(class_attr->value).find(class_name) != std::string::npos) {
            return node;
        }
    }

    GumboVector* children = &node->v.element.children;
    for (unsigned int i = 0; i < children->length; ++i) {
        GumboNode* found = find_node_by_tag_and_class(static_cast<GumboNode*>(children->data[i]), tag, class_name);
        if (found) {
            return found;
        }
    }
    return nullptr;
}


class ChatClient {
private:
    PersistenceManager db;
    string api_base = "https://api.groq.com/openai/v1/chat/completions";
    const nlohmann::json search_web_tool = {
        {"type", "function"},
        {"function", {
            {"name", "search_web"},
            {"description", "Search the web for information using DuckDuckGo Lite. Use this for recent events, specific facts, or topics outside general knowledge."},
            {"parameters", {
                {"type", "object"},
                {"properties", {
                    {"query", {
                        {"type", "string"},
                        {"description", "The search query string."}
                    }}
                }},
                {"required", {"query"}}
            }}
        }}
    };
    
    static std::string gumbo_get_text(GumboNode* node) {
        if (node->type == GUMBO_NODE_TEXT) {
            return node->v.text.text;
        }
        
        // Return empty string for non-element nodes (like comments, doctype, etc.)
        if (node->type != GUMBO_NODE_ELEMENT) {
            return "";
        }

        std::string result;
        GumboVector* children = &node->v.element.children;
        for (unsigned int i = 0; i < children->length; ++i) {
            GumboNode* child = static_cast<GumboNode*>(children->data[i]);
            // Add null check for child node before recursing
            if (child) {
                 result += gumbo_get_text(child);
            }
        }
        return result;
    }

    std::string parse_ddg_html(const std::string& html) {
        GumboOutput* output = gumbo_parse(html.c_str());
        std::string result = "Web results:\n\n";
        int count = 0;

        std::vector<GumboNode*> elements;
        std::function<void(GumboNode*)> find_tr_elements = [&](GumboNode* node) {
            if (node->type != GUMBO_NODE_ELEMENT) return;
            
            if (node->v.element.tag == GUMBO_TAG_TR) {
                elements.push_back(node);
            }

            GumboVector* children = &node->v.element.children;
            for (unsigned int i = 0; i < children->length; ++i) {
                find_tr_elements(static_cast<GumboNode*>(children->data[i]));
            }
        };

        if (output) {
            find_tr_elements(output->root);
            
            // Find the starting index of the actual results
            size_t start_index = std::string::npos; // Use npos to indicate not found
            for (size_t i = 0; i + 3 < elements.size(); ++i) { // Check potential groups of 4
                 GumboNode* potential_title_tr = elements[i];
                 GumboNode* potential_url_tr = elements[i+2];

                 if (!potential_title_tr || !potential_url_tr) continue; // Skip if nodes are invalid

                 GumboNode* a_tag = find_node_by_tag(potential_title_tr, GUMBO_TAG_A);
                 GumboNode* span_link_text = find_node_by_tag_and_class(potential_url_tr, GUMBO_TAG_SPAN, "link-text");

                 if (a_tag && span_link_text) {
                     // Found the first likely result block
                     start_index = i;
                     // std::cerr << "DEBUG: Found potential start of results at index " << start_index << "\n";
                     break; 
                 }
            }

            if (start_index == std::string::npos) {
                 // std::cerr << "DEBUG: Could not find the start of the results list.\n";
            } else {
                // Process elements in groups of 4 starting from the identified index
                for (size_t i = start_index; i + 3 < elements.size(); i += 4) { // Ensure 4 elements exist, step by 4
                    // std::cerr << "DEBUG: Processing group starting at index " << i << "\n";
                    
                    GumboNode* title_tr = elements[i];
                GumboNode* snippet_tr = elements[i+1];
                GumboNode* url_tr = elements[i+2];
                // GumboNode* separator_tr = elements[i+3]; // Separator is unused but part of the 4-step

                // Add defensive checks for node validity
                if (!title_tr || !snippet_tr || !url_tr) {
                    // std::cerr << "DEBUG: Invalid TR nodes detected in group starting at " << i << "\n";
                    continue; // Skip this group and step by 4
                }
                
                // Extract title link and text
                std::string title;
                std::string url; // URL from the title's href
                
                // Find the first <a> tag within the title_tr
                GumboNode* a_tag = find_node_by_tag(title_tr, GUMBO_TAG_A);
                
                if (a_tag) {
                    // std::cerr << "DEBUG: Found A tag within title TR\n";
                    GumboAttribute* href = gumbo_get_attribute(&a_tag->v.element.attributes, "href");
                    url = href ? href->value : "";
                    title = gumbo_get_text(a_tag);
                    // Clean up title (remove extra spaces/newlines)
                    title.erase(0, title.find_first_not_of(" \n\r\t"));
                    title.erase(title.find_last_not_of(" \n\r\t") + 1);
                } else {
                    // std::cerr << "DEBUG: No A tag found within title TR\n";
                    // Fallback: maybe the title is just the text content of the TR?
                    // title = gumbo_get_text(title_tr); // Let's skip this for now, title needs a link
                }

                // Only proceed if we found a title *from an <a> tag*
                if (!title.empty()) {
                    // Extract snippet (safer access)
                    std::string snippet = gumbo_get_text(snippet_tr); // snippet_tr is already checked for null
                    // Clean up snippet
                    snippet.erase(0, snippet.find_first_not_of(" \n\r\t"));
                    snippet.erase(snippet.find_last_not_of(" \n\r\t") + 1);


                    // Extract displayed URL text from the first <span class="link-text"> within url_tr
                    std::string url_text;
                    GumboNode* span_link_text = find_node_by_tag_and_class(url_tr, GUMBO_TAG_SPAN, "link-text");

                    if (span_link_text) {
                         // std::cerr << "DEBUG: Found span.link-text within URL TR\n";
                         url_text = gumbo_get_text(span_link_text);
                         // Clean up url_text
                         url_text.erase(0, url_text.find_first_not_of(" \n\r\t"));
                         url_text.erase(url_text.find_last_not_of(" \n\r\t") + 1);
                    } else {
                         // std::cerr << "DEBUG: No span.link-text found within URL TR\n";
                    }

                    // std::cerr << "DEBUG: Extracted URL text: " << url_text << "\n";
                    
                    // Ensure we have the title (from <a>) and the displayed URL text before adding
                    // Also limit the number of results
                    if (!url_text.empty() && count < 5) { // Limit to 5 results
                        result += std::to_string(++count) + ". " + title + "\n";
                        // Still include snippet if it exists and is not just whitespace
                        if (!snippet.empty() && snippet.find_first_not_of(" \n\r\t") != std::string::npos) {
                            result += "   " + snippet + "\n";
                        } else {
                            // Optionally indicate if snippet was missing or empty
                            // result += "   [No snippet available]\n"; 
                        }
                        result += "   " + url_text + "\n\n";
                    } else {
                         // std::cerr << "DEBUG: Skipping group starting at " << i << " due to missing title/url_text\n";
                    }
                } else {
                     // std::cerr << "DEBUG: Skipping group starting at " << i << " due to missing title\n";
                }
                // The loop automatically increments i by 4
            } // End of main processing loop (starting from start_index)
        } // End of else (start_index was found)

        // std::cerr << "DEBUG: Total results parsed: " << count << "\n";
        gumbo_destroy_output(&kGumboDefaultOptions, output);
    } // End of if (output)

        return count > 0 ? result : "No relevant results found";
    }

    std::string search_web(const std::string& query) {
        CURL* curl = curl_easy_init();
        if (!curl) throw std::runtime_error("Failed to initialize CURL");
        
        // Disable verbose logging (commented out)
        // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        
        std::string response;
        std::string url = "https://lite.duckduckgo.com/lite/";
        
        // Set headers
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:128.0) Gecko/20100101 Firefox/128.0");
        headers = curl_slist_append(headers, "Referer: https://lite.duckduckgo.com/");
        headers = curl_slist_append(headers, "Origin: https://lite.duckduckgo.com");
        headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");

        // Prepare POST data
        std::string post_data = "q=" + std::string(curl_easy_escape(curl, query.c_str(), query.length())) + 
                              "&kl=wt-wt&df=";

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        
        CURLcode res = curl_easy_perform(curl);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        
        if (res != CURLE_OK) {
            throw std::runtime_error("Search failed: " + std::string(curl_easy_strerror(res)));
        }

        // Save raw HTML for inspection (commented out)
        // std::ofstream debug_file("debug.html");
        // debug_file << response;
        // debug_file.close();

        return parse_ddg_html(response);
    }
    
    // Modified to optionally include tools
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
        payload["model"] = "llama-3.3-70b-versatile";
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
            payload["tools"] = nlohmann::json::array({search_web_tool});
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

public:
    void run() {
        cout << "LLM CLI - Type your message (Ctrl+D to exit)\n";
        
        while (true) {
            // Read input with readline for better line editing
            char* input_cstr = readline("> ");
            if (!input_cstr) break; // Exit on Ctrl+D
            
            string input(input_cstr);
            free(input_cstr);
            
            if (input.empty()) continue;

            // Handle search commands
            size_t search_pos = input.find("/search ");
            if (search_pos != string::npos) {
                try {
                    string query = input.substr(search_pos + 8);
                    cout << "\n" << search_web(query) << "\n\n";
                    continue;
                } catch(const exception& e) {
                    cerr << "Search error: " << e.what() << "\n";
                    continue;
                }
            }

            try {
                // Persist user message
                db.saveUserMessage(input);
                
                // Get context for the first API call
                auto context = db.getContextHistory(10);
                
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
                
                // Check if the response contains tool calls
                if (response_message.contains("tool_calls") && !response_message["tool_calls"].is_null()) {
                    // Save the assistant's message requesting tool use
                    // Store the whole message object containing tool_calls as a JSON string
                    db.saveAssistantMessage(response_message.dump()); 

                    // Prepare for the second API call by adding the assistant's request to context
                    context = db.getContextHistory(10); // Reload context including the tool call message

                    // Execute tools and collect results
                    for (const auto& tool_call : response_message["tool_calls"]) {
                        if (!tool_call.contains("id") || !tool_call.contains("function") || !tool_call["function"].contains("name") || !tool_call["function"].contains("arguments")) {
                             cerr << "Error: Malformed tool_call object received.\n";
                             continue;
                        }
                        string tool_call_id = tool_call["id"];
                        string function_name = tool_call["function"]["name"];
                        string function_args_str = tool_call["function"]["arguments"];
                        nlohmann::json function_args;
                        try {
                             function_args = nlohmann::json::parse(function_args_str);
                        } catch (const nlohmann::json::parse_error& e) {
                             cerr << "JSON Parsing Error (Tool Arguments): " << e.what() << "\nArgs were: " << function_args_str << "\n";
                             continue;
                        }


                        if (function_name == "search_web") {
                            if (!function_args.contains("query")) {
                                cerr << "Error: 'query' argument missing for search_web tool.\n";
                                continue;
                            }
                            string query = function_args["query"];
                            cout << "[Searching web for: " << query << "]\n"; // Inform user
                            cout.flush(); // Flush immediately after printing the search message
                            string search_result;
                            try {
                                search_result = search_web(query);
                            } catch (const std::exception& e) {
                                cerr << "Web search failed: " << e.what() << "\n";
                                search_result = "Error performing web search."; // Provide error feedback to LLM
                            }
                            
                            // Prepare tool result message content as JSON string
                            nlohmann::json tool_result_content;
                            tool_result_content["tool_call_id"] = tool_call_id;
                            tool_result_content["name"] = function_name;
                            tool_result_content["content"] = search_result; // This is the actual result string

                            // Save the tool's response message using the dedicated function
                            db.saveToolMessage(tool_result_content.dump()); 

                            // Add tool response to context for the next API call
                            context = db.getContextHistory(10); // Reload context including the tool result
                        } else {
                             cerr << "Error: Unknown tool requested: " << function_name << "\n";
                             // Optionally save an error message as a tool response?
                             // For now, just log and continue
                        }
                    }

                    // Make the second API call with the tool results included in the context
                    string final_response_str = makeApiCall(context, false); // Tools not needed for the final response generation
                    nlohmann::json final_response_json;
                     try {
                        final_response_json = nlohmann::json::parse(final_response_str);
                    } catch (const nlohmann::json::parse_error& e) {
                        cerr << "JSON Parsing Error (Second Response): " << e.what() << "\nResponse was: " << final_response_str << "\n";
                        continue; // Skip processing this turn
                    }

                    if (!final_response_json.contains("choices") || final_response_json["choices"].empty() || !final_response_json["choices"][0].contains("message") || !final_response_json["choices"][0]["message"].contains("content")) {
                        cerr << "Error: Invalid API response structure (Second Response).\nResponse was: " << final_response_str << "\n";
                        continue;
                    }
                    string final_content = final_response_json["choices"][0]["message"]["content"];

                    // Save the final assistant response
                    db.saveAssistantMessage(final_content);

                    // Display final response
                    cout << final_content << "\n\n";
                    cout.flush(); // Ensure output is displayed before next prompt

                } else {
                    // No tool calls, handle as a regular response
                    if (!response_message.contains("content") || response_message["content"].is_null()) {
                         cerr << "Error: API response missing content.\nResponse was: " << first_api_response_str << "\n";
                         continue;
                    }
                    string final_content = response_message["content"];
                    db.saveAssistantMessage(final_content);
                    cout << final_content << "\n\n";
                    cout.flush(); // Ensure output is displayed before next prompt
                }

            } catch (const nlohmann::json::parse_error& e) {
                 cerr << "JSON Parsing Error: " << e.what() << "\n";
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
