#include "tools.h"
#include "database.h" // Include database.h for PersistenceManager definition
#include <stdexcept>
#include <iostream> // For cerr/cout in tool execution feedback
#include <curl/curl.h>
#include <gumbo.h>
#include <sstream>
#include <vector>
#include <functional> // For std::function in parse_ddg_html
#include <fstream>   // For debug file saving (optional)
#include <chrono>    // For get_current_datetime
#include <ctime>     // For get_current_datetime formatting
#include <iomanip>   // For get_current_datetime formatting

// Gumbo header is now included via tools.h, remove redundant include and guards
// #ifndef GUMBO_TAG_ENUMS_DEFINED
// #define GUMBO_TAG_ENUMS_DEFINED
// #include <gumbo.h> 
// #endif


// --- Static Helper Implementations ---

size_t ToolManager::WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t total_size = size * nmemb;
    output->append((char*)contents, total_size);
    return total_size;
}

GumboNode* ToolManager::find_node_by_tag(GumboNode* node, int tag_enum) {
     GumboTag tag = static_cast<GumboTag>(tag_enum); // Cast int back to GumboTag
    if (!node || node->type != GUMBO_NODE_ELEMENT) {
        return nullptr;
    }

    if (node->v.element.tag == tag) {
        return node;
    }

    GumboVector* children = &node->v.element.children;
    for (unsigned int i = 0; i < children->length; ++i) {
        GumboNode* found = find_node_by_tag(static_cast<GumboNode*>(children->data[i]), tag_enum);
        if (found) {
            return found;
        }
    }
    return nullptr;
}

GumboNode* ToolManager::find_node_by_tag_and_class(GumboNode* node, int tag_enum, const std::string& class_name) {
    GumboTag tag = static_cast<GumboTag>(tag_enum); // Cast int back to GumboTag
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
        GumboNode* found = find_node_by_tag_and_class(static_cast<GumboNode*>(children->data[i]), tag_enum, class_name);
        if (found) {
            return found;
        }
    }
    return nullptr;
}

std::string ToolManager::gumbo_get_text(GumboNode* node) {
    if (!node) return ""; // Add null check

    if (node->type == GUMBO_NODE_TEXT) {
        return node->v.text.text;
    }
    
    if (node->type != GUMBO_NODE_ELEMENT) {
        return "";
    }

    // Skip script and style elements
    if (node->v.element.tag == GUMBO_TAG_SCRIPT || node->v.element.tag == GUMBO_TAG_STYLE) {
        return "";
    }

    std::string result;
    GumboVector* children = &node->v.element.children;
    for (unsigned int i = 0; i < children->length; ++i) {
        GumboNode* child = static_cast<GumboNode*>(children->data[i]);
        // Recursive call already handles null check via the start of this function
        result += gumbo_get_text(child);
    }
    return result;
}


// --- ToolManager Constructor (Initialize Tool Definitions) ---

ToolManager::ToolManager() :
    search_web_tool({
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
    }),
    get_current_datetime_tool({
        {"type", "function"},
        {"function", {
            {"name", "get_current_datetime"},
            {"description", "Get the current date and time."},
            {"parameters", { // No parameters needed
                {"type", "object"},
                {"properties", {}}
            }}
        }}
    }),
    visit_url_tool({
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
    }),
    read_history_tool({
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
    })
{} // End Constructor


// --- ToolManager Public Methods ---

nlohmann::json ToolManager::get_tool_definitions() const {
    // Return all defined tools in a JSON array
    return nlohmann::json::array({search_web_tool, get_current_datetime_tool, visit_url_tool, read_history_tool});
}

std::string ToolManager::execute_tool(PersistenceManager& db, const std::string& tool_name, const nlohmann::json& args) {
    // Execute the appropriate tool based on name
    if (tool_name == "search_web") {
        std::string query = args.value("query", "");
        if (query.empty()) {
            throw std::runtime_error("'query' argument missing or empty for search_web tool.");
        }
        std::cout << "[Searching web for: " << query << "]\n";
        std::cout.flush();
        try {
            return search_web(query);
        } catch (const std::exception& e) {
            std::cerr << "Web search failed: " << e.what() << "\n";
            return "Error performing web search: " + std::string(e.what()); // Return error message
        }
    } else if (tool_name == "get_current_datetime") {
        std::cout << "[Getting current date and time]\n";
        std::cout.flush();
        try {
            return get_current_datetime();
        } catch (const std::exception& e) {
            std::cerr << "Getting date/time failed: " << e.what() << "\n";
            return "Error getting current date and time.";
        }
    } else if (tool_name == "visit_url") {
        std::string url_to_visit = args.value("url", "");
        if (url_to_visit.empty()) {
             throw std::runtime_error("'url' argument missing or empty for visit_url tool.");
        }
        std::cout << "[Visiting URL: " << url_to_visit << "]\n";
        std::cout.flush();
        try {
            return visit_url(url_to_visit);
        } catch (const std::exception& e) {
            std::cerr << "URL visit failed: " << e.what() << "\n";
            return "Error visiting URL: " + std::string(e.what());
        }
    } else if (tool_name == "read_history") {
        std::string start_time = args.value("start_time", "");
        std::string end_time = args.value("end_time", "");
        size_t limit = args.value("limit", 50); // Default limit 50

        if (start_time.empty() || end_time.empty()) {
             throw std::runtime_error("'start_time' or 'end_time' missing for read_history tool.");
        }
        std::cout << "[Reading history (" << start_time << " to " << end_time << ", Limit: " << limit << ")]\n";
        std::cout.flush();
        try {
            // Pass the db reference to the implementation
            return read_history(db, start_time, end_time, limit); 
        } catch (const std::exception& e) {
            std::cerr << "History read failed: " << e.what() << "\n";
            return "Error reading history: " + std::string(e.what());
        }
    } else {
        std::cerr << "Error: Unknown tool requested: " << tool_name << "\n";
        throw std::runtime_error("Unknown tool requested: " + tool_name);
    }
}


// --- Tool Implementations (Moved from main.cpp) ---

std::string ToolManager::parse_ddg_html(const std::string& html) {
    GumboOutput* output = gumbo_parse(html.c_str());
    std::string result = "Web results:\n\n";
    int count = 0;

    std::vector<GumboNode*> elements;
    std::function<void(GumboNode*)> find_tr_elements = [&](GumboNode* node) {
        if (!node || node->type != GUMBO_NODE_ELEMENT) return; // Add null check
        
        if (node->v.element.tag == GUMBO_TAG_TR) {
            elements.push_back(node);
        }

        GumboVector* children = &node->v.element.children;
        for (unsigned int i = 0; i < children->length; ++i) {
            find_tr_elements(static_cast<GumboNode*>(children->data[i]));
        }
    };

    if (output && output->root) { // Added null check for output->root
        find_tr_elements(output->root);
        
        size_t start_index = std::string::npos; 
        for (size_t i = 0; i + 3 < elements.size(); ++i) { 
             GumboNode* potential_title_tr = elements[i];
             GumboNode* potential_url_tr = elements[i+2];

             if (!potential_title_tr || !potential_url_tr) continue; 

             GumboNode* a_tag = find_node_by_tag(potential_title_tr, GUMBO_TAG_A);
             GumboNode* span_link_text = find_node_by_tag_and_class(potential_url_tr, GUMBO_TAG_SPAN, "link-text");

             if (a_tag && span_link_text) {
                 start_index = i;
                 break; 
             }
        }

        if (start_index != std::string::npos) {
            for (size_t i = start_index; i + 3 < elements.size(); i += 4) { 
                GumboNode* title_tr = elements[i];
                GumboNode* snippet_tr = elements[i+1];
                GumboNode* url_tr = elements[i+2];

                if (!title_tr || !snippet_tr || !url_tr) {
                    continue; 
                }
                
                std::string title;
                std::string url; 
                
                GumboNode* a_tag = find_node_by_tag(title_tr, GUMBO_TAG_A);
                
                if (a_tag) {
                    GumboAttribute* href = gumbo_get_attribute(&a_tag->v.element.attributes, "href");
                    url = href ? href->value : "";
                    title = gumbo_get_text(a_tag);
                    title.erase(0, title.find_first_not_of(" \n\r\t"));
                    title.erase(title.find_last_not_of(" \n\r\t") + 1);
                } 

                if (!title.empty()) {
                    std::string snippet = gumbo_get_text(snippet_tr); 
                    snippet.erase(0, snippet.find_first_not_of(" \n\r\t"));
                    snippet.erase(snippet.find_last_not_of(" \n\r\t") + 1);

                    std::string url_text;
                    GumboNode* span_link_text = find_node_by_tag_and_class(url_tr, GUMBO_TAG_SPAN, "link-text");

                    if (span_link_text) {
                         url_text = gumbo_get_text(span_link_text);
                         url_text.erase(0, url_text.find_first_not_of(" \n\r\t"));
                         url_text.erase(url_text.find_last_not_of(" \n\r\t") + 1);
                    } 

                    if (!url_text.empty() && count < 5) { 
                        result += std::to_string(++count) + ". " + title + "\n";
                        if (!snippet.empty() && snippet.find_first_not_of(" \n\r\t") != std::string::npos) {
                            result += "   " + snippet + "\n";
                        }
                        result += "   " + url_text + "\n\n";
                    } 
                } 
            } 
        } 
        gumbo_destroy_output(&kGumboDefaultOptions, output);
    } 

    return count > 0 ? result : "No relevant results found";
}


std::string ToolManager::visit_url(const std::string& url_str) {
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("Failed to initialize CURL");

    std::string html_content;
    long http_code = 0;

    curl_easy_setopt(curl, CURLOPT_URL, url_str.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &html_content);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); 
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "llm-cli-tool/1.0"); 
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L); 
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); // Use with caution
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L); // Use with caution

    CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code); 
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        return "Error fetching URL: " + std::string(curl_easy_strerror(res));
    }

    if (http_code >= 400) {
        return "Error: Received HTTP status code " + std::to_string(http_code);
    }

    GumboOutput* output = gumbo_parse(html_content.c_str());
    if (!output || !output->root) {
        if (output) gumbo_destroy_output(&kGumboDefaultOptions, output);
        return "Error: Failed to parse HTML content.";
    }

    GumboNode* body = find_node_by_tag(output->root, GUMBO_TAG_BODY);
    std::string extracted_text;

    if (body) {
        extracted_text = gumbo_get_text(body);
    } else {
        extracted_text = gumbo_get_text(output->root); // Fallback
    }

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

    gumbo_destroy_output(&kGumboDefaultOptions, output);

    const size_t max_len = 4000; 
    if (extracted_text.length() > max_len) {
        extracted_text = extracted_text.substr(0, max_len) + "... [truncated]";
    }

    return extracted_text.empty() ? "No text content found." : extracted_text;
}


std::string ToolManager::search_web(const std::string& query) {
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("Failed to initialize CURL for search");
    
    std::string response;
    std::string url = "https://lite.duckduckgo.com/lite/";
    
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:128.0) Gecko/20100101 Firefox/128.0");
    headers = curl_slist_append(headers, "Referer: https://lite.duckduckgo.com/");
    headers = curl_slist_append(headers, "Origin: https://lite.duckduckgo.com");
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");

    // URL-encode the query
    char *escaped_query = curl_easy_escape(curl, query.c_str(), query.length());
    if (!escaped_query) {
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
        throw std::runtime_error("Failed to URL-encode search query");
    }
    std::string post_data = "q=" + std::string(escaped_query) + "&kl=wt-wt&df=";
    curl_free(escaped_query); // Free the escaped string

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    // Add timeout for search request
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L); // 10 second timeout
    
    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        throw std::runtime_error("Search failed: " + std::string(curl_easy_strerror(res)));
    }

    // Optional: Save raw HTML for debugging
    // std::ofstream debug_file("debug_search.html");
    // debug_file << response;
    // debug_file.close();

    return parse_ddg_html(response);
}

std::string ToolManager::get_current_datetime() {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    // Use thread-safe localtime_r or equivalent if multi-threading becomes a concern
    std::tm now_tm;
    #ifdef _WIN32
        localtime_s(&now_tm, &now_c); // Windows specific
    #else
        localtime_r(&now_c, &now_tm); // POSIX specific
    #endif
    ss << std::put_time(&now_tm, "%Y-%m-%d %H:%M:%S %Z"); 
    return ss.str();
}

// Takes PersistenceManager reference
std::string ToolManager::read_history(PersistenceManager& db, const std::string& start_time, const std::string& end_time, size_t limit) {
     std::vector<Message> messages = db.getHistoryRange(start_time, end_time, limit);
     if (messages.empty()) {
         return "No messages found between " + start_time + " and " + end_time + " (Limit: " + std::to_string(limit) + ").";
     }

     std::stringstream ss;
     ss << "History (" << start_time << " to " << end_time << ", Limit: " << limit << "):\n";
     for (const auto& msg : messages) {
         std::string truncated_content = msg.content;
         if (truncated_content.length() > 100) { 
             truncated_content = truncated_content.substr(0, 97) + "...";
         }
         size_t pos = 0;
         while ((pos = truncated_content.find('\n', pos)) != std::string::npos) {
             truncated_content.replace(pos, 1, "\\n");
             pos += 2; 
         }
         ss << "[" << msg.timestamp << " ID: " << msg.id << ", Role: " << msg.role << "] " << truncated_content << "\n";
     }
     return ss.str();
}
