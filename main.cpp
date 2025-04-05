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
#include "database.h"

using namespace std;

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, string* output) {
    size_t total_size = size * nmemb;
    output->append((char*)contents, total_size);
    return total_size;
}

class ChatClient {
private:
    PersistenceManager db;
    string api_base = "https://api.groq.com/openai/v1/chat/completions";
    
    std::string parse_ddg_html(const std::string& html) {
        GumboOutput* output = gumbo_parse(html.c_str());
        std::string result = "Web results:\n\n";
        int count = 0;

        std::function<void(GumboNode*)> search_results = [&](GumboNode* node) {
            if (node->type != GUMBO_NODE_ELEMENT) return;

            // Find result rows
            if (node->v.element.tag == GUMBO_TAG_TR) {
                GumboAttribute* class_attr = gumbo_get_attribute(&node->v.element.attributes, "class");
                if (class_attr && std::string(class_attr->value) == "result-row") {
                    GumboNode* link = nullptr;
                    GumboNode* snippet = nullptr;
                    GumboNode* url = nullptr;

                    // Iterate through table cells
                    GumboVector* children = &node->v.element.children;
                    for (unsigned int i = 0; i < children->length; ++i) {
                        GumboNode* td = static_cast<GumboNode*>(children->data[i]);
                        if (td->type == GUMBO_NODE_ELEMENT && td->v.element.tag == GUMBO_TAG_TD) {
                            GumboAttribute* class_attr = gumbo_get_attribute(&td->v.element.attributes, "class");
                            
                            if (td->v.element.children.length > 0) {
                                GumboNode* first_child = static_cast<GumboNode*>(td->v.element.children.data[0]);
                                
                                if (first_child->type == GUMBO_NODE_ELEMENT && first_child->v.element.tag == GUMBO_TAG_A) {
                                    link = first_child;
                                } else if (class_attr && std::string(class_attr->value) == "result-snippet") {
                                    snippet = td;
                                } else if (first_child->type == GUMBO_NODE_ELEMENT && first_child->v.element.tag == GUMBO_TAG_SPAN) {
                                    url = first_child;
                                }
                            }
                        }
                    }

                    // Extract components
                    if (link && snippet) {
                        GumboAttribute* href_attr = gumbo_get_attribute(&link->v.element.attributes, "href");
                        if (href_attr) {
                            std::string title;
                            std::string desc;
                            std::string href = href_attr->value;
                            
                            // Extract title from link
                            if (link->v.element.children.length > 0) {
                                GumboNode* title_node = static_cast<GumboNode*>(link->v.element.children.data[0]);
                                if (title_node->type == GUMBO_NODE_TEXT) {
                                    title = title_node->v.text.text;
                                }
                            }
                            
                            // Extract description
                            if (snippet->v.element.children.length > 0) {
                                GumboNode* desc_node = static_cast<GumboNode*>(snippet->v.element.children.data[0]);
                                if (desc_node->type == GUMBO_NODE_TEXT) {
                                    desc = desc_node->v.text.text;
                                }
                            }
                            
                            result += std::to_string(++count) + ". " + title + "\n";
                            result += "   " + desc + "\n";
                            result += "   " + href + "\n\n";
                        }
                    }
                }
            }

            // Recursively search child nodes
            GumboVector* children = &node->v.element.children;
            for (unsigned int i = 0; i < children->length; ++i) {
                search_results(static_cast<GumboNode*>(children->data[i]));
            }
        };

        if (output) {
            search_results(output->root);
            gumbo_destroy_output(&kGumboDefaultOptions, output);
        }

        return count > 0 ? result : "No relevant results found";
    }

    std::string search_web(const std::string& query) {
        CURL* curl = curl_easy_init();
        if (!curl) throw std::runtime_error("Failed to initialize CURL");
        
        std::string response;
        std::string url = "https://lite.duckduckgo.com/lite/?q=" + 
                         std::string(curl_easy_escape(curl, query.c_str(), query.length()));
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36");
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        
        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        
        if (res != CURLE_OK) {
            throw std::runtime_error("Search failed: " + std::string(curl_easy_strerror(res)));
        }

        return parse_ddg_html(response);
    }
    
    string makeApiCall(const vector<Message>& context) {
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
            payload["messages"].push_back({{"role", msg.role}, {"content", msg.content}});
        }

        string json_payload = payload.dump();
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

        auto json = nlohmann::json::parse(response);
        return json["choices"][0]["message"]["content"];
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
            if (input.rfind("/search ", 0) == 0) {  // Pre-C++20 compatible check
                try {
                    string query = input.substr(8);
                    cout << "\n" << search_web(query) << "\n\n";
                } catch(const exception& e) {
                    cerr << "Search error: " << e.what() << "\n";
                }
                continue;
            }

            try {
                // Persist user message
                db.saveUserMessage(input);
                
                // Get context for API call
                auto context = db.getContextHistory(10);
                
                // Make API call
                string ai_response = makeApiCall(context);
                
                // Persist assistant response
                db.saveAssistantMessage(ai_response);
                
                // Display response
                cout << ai_response << "\n\n"; // add spacing
                
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
