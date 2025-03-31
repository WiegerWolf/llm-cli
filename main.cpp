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
#include <vector>
#include <sqlite3.h>

using namespace std;

struct Message {
    string role;
    string content;
    int id = 0; // Default to 0 for new messages
};

struct Config {
    string api_base = "https://api.groq.com/openai/v1/chat/completions";
    vector<Message> chat_history;
    
    void ensureSystemMessage() {
        if (chat_history.empty() || chat_history[0].role != "system") {
            chat_history.insert(chat_history.begin(), {"system", "You are a helpful assistant."});
        }
    }
};

// Database functions are defined in database.cpp
extern void saveHistoryToDatabase(const vector<Message>& history);
extern vector<Message> loadHistoryFromDatabase();

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, string* output) {
    size_t total_size = size * nmemb;
    output->append((char*)contents, total_size);
    return total_size;
}

bool fetchGroqResponse(const Config& config, const string& input, string& response) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

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
    
    // Add previous conversation history
    for (const auto& msg : config.chat_history) {
        payload["messages"].push_back({{"role", msg.role}, {"content", msg.content}});
    }

    string json_payload = payload.dump();

    curl_easy_setopt(curl, CURLOPT_URL, config.api_base.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, json_payload.size());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return res == CURLE_OK;
}

int main() {
    Config config;
    config.chat_history = loadHistoryFromDatabase();
    config.ensureSystemMessage();

    cout << "LLM CLI - Type your message (Ctrl+D to exit)\n";

    while (true) {
        // Read input with readline for better line editing
        char* input_cstr = readline("> ");
        if (!input_cstr) break; // Exit on Ctrl+D
        
        string input(input_cstr);
        free(input_cstr);
        
        if (input.empty()) continue;

        try {
            // Save user message immediately
            config.chat_history.push_back({"user", input});
            saveHistoryToDatabase(config.chat_history);

            // Make API call with updated history
            string response;
            if (!fetchGroqResponse(config, input, response)) {
                // Remove failed message from history
                config.chat_history.pop_back();
                saveHistoryToDatabase(config.chat_history);
                cerr << "Error: API request failed\n";
                continue;
            }

            // Save assistant response
            auto json = nlohmann::json::parse(response);
            string ai_response = json["choices"][0]["message"]["content"];
            config.chat_history.push_back({"assistant", ai_response});
            saveHistoryToDatabase(config.chat_history);
            
            cout << "\033[1;36m"  // Start cyan color
                 << ai_response 
                 << "\033[0m\n\n"; // Reset color and add spacing

            // Keep working memory manageable
            constexpr size_t MAX_HISTORY_PAIRS = 10; // Keep last 10 exchanges
            size_t total_to_keep = 1 + (MAX_HISTORY_PAIRS * 2); // System + 10 pairs
            
            if (config.chat_history.size() > total_to_keep) {
                // Preserve system message and recent messages
                auto start = config.chat_history.begin() + 1; // After system
                auto end = config.chat_history.end();
                auto new_start = end - (total_to_keep - 1);
                
                if (new_start > start) {
                    config.chat_history.erase(start, new_start);
                }
            }
        } catch (const exception& e) {
            cerr << "Error: " << e.what() << "\n";
        }
    }

    cout << "\nExiting...\n";
    saveHistoryToDatabase(config.chat_history);
    return 0;
}
