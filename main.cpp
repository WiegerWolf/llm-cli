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
            
            // DEBUG: Show total TR elements found
            std::cerr << "DEBUG: Found " << elements.size() << " TR elements\n";
            
            // Process elements in groups of 3 (title, snippet, URL) + 1 separator
            for (size_t i = 0; i < elements.size(); ) {
                std::cerr << "DEBUG: Processing group starting at index " << i << "\n";
                
                // Check if we have at least 3 elements remaining
                if (i + 2 >= elements.size()) {
                    std::cerr << "DEBUG: Breaking - not enough elements remaining\n";
                    break;
                }

                GumboNode* title_tr = elements[i];
                GumboNode* snippet_tr = elements[i+1];
                GumboNode* url_tr = elements[i+2];
                
                // Add defensive checks for node validity
                if (!title_tr || !snippet_tr || !url_tr) {
                    std::cerr << "DEBUG: Invalid TR nodes detected\n";
                    i++;
                    continue;
                }
                
                std::cerr << "DEBUG: Title TR content: " << gumbo_get_text(title_tr) << "\n";

                // Extract title link
                GumboNode* a_tag = nullptr;
                GumboVector* title_children = &title_tr->v.element.children;
                if (title_children->length > 0) {
                    GumboNode* first_td = static_cast<GumboNode*>(title_children->data[0]);
                    if (first_td && first_td->type == GUMBO_NODE_ELEMENT && 
                        first_td->v.element.tag == GUMBO_TAG_TD && 
                        first_td->v.element.children.length > 0) {
                        a_tag = static_cast<GumboNode*>(first_td->v.element.children.data[0]);
                    }
                }

                if (a_tag && a_tag->type == GUMBO_NODE_ELEMENT && a_tag->v.element.tag == GUMBO_TAG_A) {
                    std::cerr << "DEBUG: Found A tag\n";
                    // Get href and title
                    GumboAttribute* href = gumbo_get_attribute(&a_tag->v.element.attributes, "href");
                    std::string url = href ? href->value : "";
                    std::string title = gumbo_get_text(a_tag);

                    // Extract snippet
                    std::string snippet = gumbo_get_text(snippet_tr);

                    // Extract URL from url_tr's span.link-text
                    std::string url_text;
                    GumboVector* url_children = &url_tr->v.element.children;
                    if (url_children->length > 0) {
                        GumboNode* url_td = static_cast<GumboNode*>(url_children->data[0]);
                        if (url_td && url_td->type == GUMBO_NODE_ELEMENT && 
                            url_td->v.element.tag == GUMBO_TAG_TD && 
                            url_td->v.element.children.length > 0) {
                            GumboNode* span = static_cast<GumboNode*>(url_td->v.element.children.data[0]);
                            if (span && span->type == GUMBO_NODE_ELEMENT && 
                                span->v.element.tag == GUMBO_TAG_SPAN &&
                                gumbo_get_attribute(&span->v.element.attributes, "class") &&
                                std::string(gumbo_get_attribute(&span->v.element.attributes, "class")->value) == "link-text") {
                                url_text = gumbo_get_text(span);
                            }
                        }
                    }

                    std::cerr << "DEBUG: Extracted URL text: " << url_text << "\n";
                    
                    if (!title.empty() && !url_text.empty()) {
                        result += std::to_string(++count) + ". " + title + "\n";
                        if (!snippet.empty()) {
                            result += "   " + snippet + "\n";
                        }
                        result += "   " + url_text + "\n\n";
                    }
                }

                // Move to next result group (skip separator TR if present)
                size_t step = 3;
                if (i + 3 < elements.size()) {
                    GumboNode* separator = elements[i+3];
                    if (separator && separator->type == GUMBO_NODE_ELEMENT && 
                        separator->v.element.children.length == 0) {
                        step = 4;
                        std::cerr << "DEBUG: Found separator, stepping by 4\n";
                    }
                }
                
                std::cerr << "DEBUG: Stepping by " << step << " positions\n";
                i += step;
            }

            std::cerr << "DEBUG: Total results found: " << count << "\n";
            gumbo_destroy_output(&kGumboDefaultOptions, output);
        }

        return count > 0 ? result : "No relevant results found";
    }

    std::string search_web(const std::string& query) {
        CURL* curl = curl_easy_init();
        if (!curl) throw std::runtime_error("Failed to initialize CURL");
        
        // Enable verbose logging
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        
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

        // Save raw HTML for inspection
        std::ofstream debug_file("debug.html");
        debug_file << response;
        debug_file.close();

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
