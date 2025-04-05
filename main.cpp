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
    
    static std::string gumbo_get_text(GumboNode* node) {
        if (node->type == GUMBO_NODE_TEXT) {
            return node->v.text.text;
        }
        
        std::string result;
        GumboVector* children = &node->v.element.children;
        for (unsigned int i = 0; i < children->length; ++i) {
            result += gumbo_get_text(static_cast<GumboNode*>(children->data[i]));
        }
        return result;
    }

    std::string parse_ddg_html(const std::string& html) {
        GumboOutput* output = gumbo_parse(html.c_str());
        std::string result = "Web results:\n\n";
        int count = 0;

        std::function<void(GumboNode*)> parse_node = [&](GumboNode* node) {
            if (node->type != GUMBO_NODE_ELEMENT) return;

            // Detect result groups
            if (node->v.element.tag == GUMBO_TAG_TR) {
                GumboVector* children = &node->v.element.children;
                
                // Result title row
                if (children->length > 0) {
                    GumboNode* first_td = static_cast<GumboNode*>(children->data[0]);
                    if (first_td->v.element.tag == GUMBO_TAG_TD) {
                        GumboNode* a_tag = nullptr;
                        if (first_td->v.element.children.length > 0) {
                            a_tag = static_cast<GumboNode*>(first_td->v.element.children.data[0]);
                        }
                        
                        if (a_tag && a_tag->v.element.tag == GUMBO_TAG_A) {
                            // Found a result entry
                            std::string title, url, snippet;

                            // Extract title and URL
                            GumboAttribute* href = gumbo_get_attribute(&a_tag->v.element.attributes, "href");
                            if (href) {
                                url = href->value;
                                if (a_tag->v.element.children.length > 0) {
                                    GumboNode* text_node = static_cast<GumboNode*>(a_tag->v.element.children.data[0]);
                                    if (text_node->type == GUMBO_NODE_TEXT) {
                                        title = text_node->v.text.text;
                                    }
                                }
                            }

                            // Look for sibling TR elements containing snippet and URL
                            GumboNode* parent = static_cast<GumboNode*>(node->parent);
                            if (parent && parent->v.element.tag == GUMBO_TAG_TBODY) {
                                size_t index = 0;
                                while (index < parent->v.element.children.length && 
                                       static_cast<GumboNode*>(parent->v.element.children.data[index]) != node) {
                                    index++;
                                }

                                // Next TR contains snippet
                                if (index + 1 < parent->v.element.children.length) {
                                    GumboNode* snippet_tr = static_cast<GumboNode*>(parent->v.element.children.data[index + 1]);
                                    if (snippet_tr->v.element.tag == GUMBO_TAG_TR) {
                                        GumboNode* snippet_td = static_cast<GumboNode*>(snippet_tr->v.element.children.data[0]);
                                        if (snippet_td->v.element.tag == GUMBO_TAG_TD) {
                                            snippet = gumbo_get_text(snippet_td);
                                        }
                                    }
                                }

                                // Next+1 TR contains URL
                                if (index + 2 < parent->v.element.children.length) {
                                    GumboNode* url_tr = static_cast<GumboNode*>(parent->v.element.children.data[index + 2]);
                                    if (url_tr->v.element.tag == GUMBO_TAG_TR) {
                                        GumboNode* url_td = static_cast<GumboNode*>(url_tr->v.element.children.data[0]);
                                        if (url_td->v.element.tag == GUMBO_TAG_TD) {
                                            std::string full_text = gumbo_get_text(url_td);
                                            size_t space_pos = full_text.find(' ');
                                            if (space_pos != std::string::npos) {
                                                url = full_text.substr(space_pos + 1);
                                            }
                                        }
                                    }
                                }
                            }

                            if (!title.empty() && !url.empty()) {
                                result += std::to_string(++count) + ". " + title + "\n";
                                if (!snippet.empty()) {
                                    result += "   " + snippet + "\n";
                                }
                                result += "   " + url + "\n\n";
                            }
                        }
                    }
                }
            }

            // Recursively process child nodes
            GumboVector* children = &node->v.element.children;
            for (unsigned int i = 0; i < children->length; ++i) {
                parse_node(static_cast<GumboNode*>(children->data[i]));
            }
        };

        if (output) {
            parse_node(output->root);
            gumbo_destroy_output(&kGumboDefaultOptions, output);
        }

        return count > 0 ? result : "No relevant results found";
    }

    std::string search_web(const std::string& query) {
        CURL* curl = curl_easy_init();
        if (!curl) throw std::runtime_error("Failed to initialize CURL");
        
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
