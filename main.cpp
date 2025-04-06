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
#include <chrono> // For date/time
#include <ctime>  // For date/time formatting
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
    const nlohmann::json get_current_datetime_tool = {
        {"type", "function"},
        {"function", {
            {"name", "get_current_datetime"},
            {"description", "Get the current date and time."},
            {"parameters", { // No parameters needed
                {"type", "object"},
                {"properties", {}}
            }}
        }}
    };
    const nlohmann::json visit_url_tool = {
        {"type", "function"},
        {"function", {
            {"name", "visit_url"},
            {"description", "Fetch the main text content of a given URL."},
            {"parameters", {
                {"type", "object"},
                {"properties", {
                    {"url", {
                        {"type", "string"},
                        {"description", "The full URL to visit (including http:// or https://)."}
                    }}
                }},
                {"required", {"url"}}
            }}
        }}
    };
    const nlohmann::json read_history_tool = {
        {"type", "function"},
        {"function", {
            {"name", "read_history"},
            {"description", "Read past messages from the conversation history database within a specified time range."},
            {"parameters", {
                {"type", "object"},
                {"properties", {
                    {"start_time", {
                        {"type", "string"},
                        {"description", "The start timestamp (inclusive) in 'YYYY-MM-DD HH:MM:SS' format."}
                    }},
                    {"end_time", {
                        {"type", "string"},
                        {"description", "The end timestamp (inclusive) in 'YYYY-MM-DD HH:MM:SS' format."}
                    }},
                    {"limit", {
                        {"type", "integer"},
                        {"description", "The maximum number of messages to retrieve within the range."},
                        {"default", 50} // Default limit if not specified
                    }}
                }},
                {"required", {"start_time", "end_time"}} // Require time range
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

    // Function to fetch URL content and extract text
    std::string visit_url(const std::string& url_str) {
        CURL* curl = curl_easy_init();
        if (!curl) throw std::runtime_error("Failed to initialize CURL");

        std::string html_content;
        long http_code = 0;

        // Set common CURL options
        curl_easy_setopt(curl, CURLOPT_URL, url_str.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &html_content);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // Follow redirects
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "llm-cli-tool/1.0"); // Simple user agent
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L); // 15 second timeout
        // Disable SSL verification for simplicity (use with caution!)
        // Consider adding proper certificate handling if needed for production
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

        CURLcode res = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code); // Get HTTP status code
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            return "Error fetching URL: " + std::string(curl_easy_strerror(res));
        }

        if (http_code >= 400) {
            return "Error: Received HTTP status code " + std::to_string(http_code);
        }

        // Parse the HTML using Gumbo
        GumboOutput* output = gumbo_parse(html_content.c_str());
        if (!output || !output->root) {
            if (output) gumbo_destroy_output(&kGumboDefaultOptions, output);
            return "Error: Failed to parse HTML content.";
        }

        // Find the body node
        GumboNode* body = find_node_by_tag(output->root, GUMBO_TAG_BODY);
        std::string extracted_text;

        if (body) {
            // Extract all text content from the body
            extracted_text = gumbo_get_text(body);
            // Basic cleanup: remove excessive whitespace/newlines
            std::stringstream ss_in(extracted_text);
            std::string segment;
            std::stringstream ss_out;
            bool first = true;
            while (ss_in >> segment) {
                if (!first) ss_out << " ";
                ss_out << segment;
                first = false;
            }
            extracted_text = ss_out.str();

        } else {
            // Fallback: try getting text from the root if body not found
            extracted_text = gumbo_get_text(output->root);
             std::stringstream ss_in(extracted_text);
            std::string segment;
            std::stringstream ss_out;
            bool first = true;
            while (ss_in >> segment) {
                if (!first) ss_out << " ";
                ss_out << segment;
                first = false;
            }
            extracted_text = ss_out.str();
        }

        gumbo_destroy_output(&kGumboDefaultOptions, output);

        // Limit the length to avoid overwhelming the context
        const size_t max_len = 4000; // Limit to ~4000 characters
        if (extracted_text.length() > max_len) {
            extracted_text = extracted_text.substr(0, max_len) + "... [truncated]";
        }


        return extracted_text.empty() ? "No text content found." : extracted_text;
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
            // Include all defined tools in the array
            payload["tools"] = nlohmann::json::array({search_web_tool, get_current_datetime_tool, visit_url_tool, read_history_tool}); 
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

private: // Tool implementations and helpers
    // Function to get current date and time as a string
    std::string get_current_datetime() {
        auto now = std::chrono::system_clock::now();
        auto now_c = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        // Format: YYYY-MM-DD HH:MM:SS Timezone (e.g., 2024-04-06 15:30:00 PDT)
        // Using std::put_time requires #include <iomanip> which is already included
        // Using %Z for timezone name might be locale-dependent. %z for offset is more standard.
        ss << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H:%M:%S %Z"); 
        return ss.str();
    }

    // Updated function to read history time range and format it
    std::string read_history(const std::string& start_time, const std::string& end_time, size_t limit) {
         std::vector<Message> messages = db.getHistoryRange(start_time, end_time, limit);
         if (messages.empty()) {
             return "No messages found between " + start_time + " and " + end_time + " (Limit: " + std::to_string(limit) + ").";
         }

         std::stringstream ss;
         ss << "History (" << start_time << " to " << end_time << ", Limit: " << limit << "):\n";
         for (const auto& msg : messages) {
             // Basic formatting, truncate long content
             std::string truncated_content = msg.content;
             if (truncated_content.length() > 100) { // Limit content length in output
                 truncated_content = truncated_content.substr(0, 97) + "...";
             }
             // Replace newlines in content to keep output clean
             size_t pos = 0;
             while ((pos = truncated_content.find('\n', pos)) != std::string::npos) {
                 truncated_content.replace(pos, 1, "\\n");
                 pos += 2; // Move past the replaced "\\n"
             }
             // Include timestamp in the output
             ss << "[" << msg.timestamp << " ID: " << msg.id << ", Role: " << msg.role << "] " << truncated_content << "\n";
         }
         return ss.str();
    }


    // Helper function to execute a tool, get final response, and handle persistence/output
    bool handleToolExecutionAndFinalResponse(
        const std::string& tool_call_id,
        const std::string& function_name,
        const nlohmann::json& function_args,
        std::vector<Message>& context // Pass context by reference to update it
    ) {
        string tool_result_str; // Renamed from search_result for clarity
        if (function_name == "search_web") {
            string query = function_args.value("query", ""); // Use .value for safety
            if (query.empty()) {
                 cerr << "Error: 'query' argument missing or empty for search_web tool.\n";
                 return false; // Indicate failure
            }
            cout << "[Searching web for: " << query << "]\n";
            cout.flush(); // Flush immediately
            try {
                tool_result_str = search_web(query);
            } catch (const std::exception& e) {
                cerr << "Web search failed: " << e.what() << "\n";
                tool_result_str = "Error performing web search."; // Provide error feedback to LLM
            }
        } else if (function_name == "get_current_datetime") {
             // No arguments needed for this tool
             cout << "[Getting current date and time]\n"; // Inform user
             cout.flush();
             try {
                 tool_result_str = get_current_datetime();
             } catch (const std::exception& e) { // Should be unlikely, but good practice
                 cerr << "Getting date/time failed: " << e.what() << "\n";
                 tool_result_str = "Error getting current date and time.";
             }
        } else if (function_name == "visit_url") {
             string url_to_visit = function_args.value("url", "");
             if (url_to_visit.empty()) {
                 cerr << "Error: 'url' argument missing or empty for visit_url tool.\n";
                 tool_result_str = "Error: URL parameter is missing.";
             } else {
                 cout << "[Visiting URL: " << url_to_visit << "]\n"; // Inform user
                 cout.flush();
                 try {
                     tool_result_str = visit_url(url_to_visit);
                 } catch (const std::exception& e) {
                     cerr << "URL visit failed: " << e.what() << "\n";
                     tool_result_str = "Error visiting URL: " + std::string(e.what());
                 }
             }
        } else if (function_name == "read_history") {
             // Get time range and limit
             std::string start_time = function_args.value("start_time", "");
             std::string end_time = function_args.value("end_time", "");
             size_t limit = function_args.value("limit", 50); // Default limit 50

             if (start_time.empty() || end_time.empty()) {
                  cerr << "Error: 'start_time' or 'end_time' missing for read_history tool.\n";
                  tool_result_str = "Error: Both start_time and end_time are required.";
             } else {
                  cout << "[Reading history (" << start_time << " to " << end_time << ", Limit: " << limit << ")]\n"; // Inform user
                  cout.flush();
                  try {
                      tool_result_str = read_history(start_time, end_time, limit);
                  } catch (const std::exception& e) {
                      cerr << "History read failed: " << e.what() << "\n";
                 tool_result_str = "Error reading history: " + std::string(e.what());
             }
           } // <-- Added missing brace for the 'else' part of read_history check
        } else {
            cerr << "Error: Unknown tool requested: " << function_name << "\n";
            tool_result_str = "Error: Unknown tool requested."; // Provide error feedback
            // We still need to save this error as a tool response
        }

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
        cout << "LLM CLI - Type your message (Ctrl+D to exit)\n";
        static int synthetic_tool_call_counter = 0; // Counter for synthetic IDs
        
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

                        // Call the helper function - it handles execution, saving result, second call, saving/printing final response
                        if (handleToolExecutionAndFinalResponse(tool_call_id, function_name, function_args, context)) {
                             tool_call_flow_completed = true; // Mark success for at least one tool
                        } else {
                             // Error was already printed by the helper.
                             // Decide if we should stop processing further tool calls in this turn? For now, continue.
                        }
                        // Context is updated within the helper after saving tool result
                    }
                // --- Path 2: <function=...> in content ---
                } else if (response_message.contains("content") && response_message["content"].is_string()) {
                    std::string content_str = response_message["content"];
                    // Look for <function>NAME{ARGS}</function> format
                    size_t func_start = content_str.find("<function>");
                    size_t func_end = content_str.rfind("</function>");

                    // Check if the tags exist and are in the correct order
                    if (func_start != std::string::npos && func_end != std::string::npos && func_end > func_start) {
                        size_t name_start = func_start + 10; // Position after "<function>"
                        // Find the start of the arguments JSON (the first '{' after the name)
                        size_t args_start = content_str.find('{', name_start);
                        // Find the end of the arguments JSON (the last '}' before the end tag)
                        size_t args_end = content_str.rfind('}', func_end);
                        // The function name ends just before the arguments start
                        size_t name_end = args_start; 

                        // Validate the found positions: name must exist, args must exist and be valid JSON boundaries
                        if (args_start != std::string::npos && args_end != std::string::npos && args_end > args_start &&
                            name_end > name_start) // Ensure name is not empty
                        {
                            std::string function_name = content_str.substr(name_start, name_end - name_start);
                            // Trim potential whitespace from name
                            function_name.erase(0, function_name.find_first_not_of(" \n\r\t"));
                            function_name.erase(function_name.find_last_not_of(" \n\r\t") + 1);

                            std::string args_str = content_str.substr(args_start, args_end - args_start + 1); // Include '{' and '}'

                            // Handle known functions (currently only search_web via this path)
                            if (function_name == "search_web") {
                                try {
                                    nlohmann::json function_args = nlohmann::json::parse(args_str);
                                    if (function_args.contains("query")) {
                                        // Save the original assistant message containing <function=...>
                                        db.saveAssistantMessage(content_str);
                                        context = db.getContextHistory(); // Reload context

                                        // Generate synthetic ID
                                        std::string tool_call_id = "synth_" + std::to_string(++synthetic_tool_call_counter);

                                        // Call the helper function
                                        if (handleToolExecutionAndFinalResponse(tool_call_id, function_name, function_args, context)) {
                                            tool_call_flow_completed = true; // Mark success
                                        } else {
                                            // Error handled by helper
                                        }
                                    } else {
                                         cerr << "Warning: Parsed <function=search_web> but 'query' missing in args: " << args_str << "\n";
                                         // Fall through to treat as regular message below
                                    }
                                } catch (const nlohmann::json::parse_error& e) {
                                    cerr << "Warning: Failed to parse arguments from <function=...>: " << e.what() << "\nArgs were: " << args_str << "\n";
                                    // Fall through to treat as regular message below
                                }
                            } else {
                                 cerr << "Warning: Unsupported function name in <function=...>: " << function_name << "\n";
                                 // Fall through to treat as regular message below
                            }
                        } else {
                             // Malformed <function=...> tag, treat as regular message below
                        }
                    }
                    
                    // --- Path 3: Regular Response (if not tool_calls and not handled <function=...>) ---
                    if (!tool_call_flow_completed) {
                         // Content is already known to be a non-null string here
                         db.saveAssistantMessage(content_str);
                         cout << content_str << "\n\n";
                         cout.flush();
                    }
                } else if (!tool_call_flow_completed) { 
                    // Handle cases where content might be null or not a string, and no tool calls occurred
                    if (response_message.contains("content") && !response_message["content"].is_null()) {
                         // It has content, but it's not a string (shouldn't happen often with LLMs, but handle defensively)
                         std::string non_string_content = response_message["content"].dump(); // Save/show the JSON representation
                         db.saveAssistantMessage(non_string_content);
                         cout << non_string_content << "\n\n";
                         cout.flush();
                    } else {
                         // No tool calls and content is null or missing
                         cerr << "Error: API response missing content and tool_calls.\nResponse was: " << first_api_response_str << "\n";
                         // No message to save or print, just continue to next prompt
                    }
                }
                // If tool_call_flow_completed is true, the helper function already printed the final response.
                // If it's false, the regular response was printed above (or an error occurred).
                // Either way, we are ready for the next user input.

            } catch (const nlohmann::json::parse_error& e) {
                 cerr << "JSON Parsing Error (Outer Loop): " << e.what() << "\n";
            } catch (const exception& e) {
                cerr << "Error: " << e.what() << "\n";
            }
        }
    }
}; // <-- Added missing brace for ChatClient class


int main() {
    ChatClient client;
    client.run();
    cout << "\nExiting...\n";
    return 0;
}
