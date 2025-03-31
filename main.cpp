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
#include <fstream>
#include <filesystem>

using namespace std;

struct Message {
    string role;
    string content;
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

string getHistoryPath() {
    const char* home = getenv("HOME");
    return string(home) + "/.llm-cli-history.json";
}

void saveHistory(const vector<Message>& history) {
    nlohmann::json j;
    for (const auto& msg : history) {
        j.push_back({{"role", msg.role}, {"content", msg.content}});
    }
    
    ofstream file(getHistoryPath());
    if (file) {
        file << j.dump(4);
    }
}

vector<Message> loadHistory() {
    vector<Message> history;
    ifstream file(getHistoryPath());
    
    if (file) {
        try {
            nlohmann::json j;
            file >> j;
            for (const auto& item : j) {
                history.push_back({item["role"], item["content"]});
            }
        } catch (...) {
            // Invalid history file, start fresh
        }
    }
    
    // Add system prompt if no history found
    if (history.empty()) {
        history.push_back({"system", "You are a helpful assistant."});
    }
    
    return history;
}

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
    // Add new user message
    payload["messages"].push_back({{"role", "user"}, {"content", input}});

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
    config.chat_history = loadHistory();
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
            string response;
            if (!fetchGroqResponse(config, input, response)) {
                cerr << "Error: API request failed\n";
                continue;
            }

            auto json = nlohmann::json::parse(response);
            string ai_response = json["choices"][0]["message"]["content"];
            
            // Add to chat history and print response
            config.chat_history.push_back({"user", input});
            config.chat_history.push_back({"assistant", ai_response});
            
            cout << "\033[1;36m"  // Start cyan color
                 << ai_response 
                 << "\033[0m\n\n"; // Reset color and add spacing

            // Keep history manageable
            if (config.chat_history.size() > 20) {
                config.chat_history.erase(
                    config.chat_history.begin(),
                    config.chat_history.begin() + 2
                );
            }
        } catch (const exception& e) {
            cerr << "Error: " << e.what() << "\n";
        }
    }

    cout << "\nExiting...\n";
    saveHistory(config.chat_history);
    return 0;
}
