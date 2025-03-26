#include <iostream>
#include <string>
#include <curl/curl.h>
#include <ftxui/screen/screen.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <cstdlib> // For getenv()
#include <nlohmann/json.hpp>

using namespace ftxui;

struct Config {
    std::string provider;
    std::string model;
    std::string api_base = "https://api.groq.com/openai/v1/chat/completions";
};

struct Message {
    std::string role;
    std::string content;
};
std::vector<Message> chat_history;

// Callback to handle HTTP response
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t total_size = size * nmemb;
    output->append((char*)contents, total_size);
    return total_size;
}

bool fetchUrl(const std::string& url, std::string& response) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    
    // Get API key from environment
    const char* api_key = std::getenv("GROQ_API_KEY");
    if (!api_key) {
        throw std::runtime_error("GROQ_API_KEY environment variable not set!");
    }
    std::string auth_header = "Authorization: Bearer " + std::string(api_key ? api_key : "");
    struct curl_slist* headers = curl_slist_append(nullptr, auth_header.c_str());
    if (!headers) return false;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    // Build JSON payload
    nlohmann::json payload;
    payload["model"] = "llama-3.3-70b-versatile";
    payload["messages"] = nlohmann::json::array();
    for (const auto& msg : chat_history) {
        payload["messages"].push_back({
            {"role", msg.role},
            {"content", msg.content}
        });
    }

    std::string json_payload = payload.dump();

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "llm-cli/1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, json_payload.size());

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    return res == CURLE_OK;
}

Config show_selection_menu() {
    Config config;
    auto screen = ScreenInteractive::Fullscreen();
    
    // Provider selection (auto-select on enter)
    std::vector<std::string> providers = {"Groq"};
    int selected_provider = 0;
    auto provider_menu = Menu(&providers, &selected_provider)
        | CatchEvent([&](Event event) {
            if (event == Event::Return) screen.Exit();
            return false;
          });
    screen.Loop(provider_menu);
    config.provider = providers[selected_provider];

    // Model selection (auto-select on enter)
    std::vector<std::string> models = {"llama-3-70b-8192"}; 
    int selected_model = 0;
    auto model_menu = Menu(&models, &selected_model)
        | CatchEvent([&](Event event) {
            if (event == Event::Return) screen.Exit();
            return false;
          });
    screen.Loop(model_menu);
    config.model = models[selected_model];
    return config;
}

Component ChatInterface() {
    std::string input;
    auto input_field = Input(&input, "Type your message...") 
        | CatchEvent([&](Event event) {
            return event.is_mouse();
          }); // Disable mouse in input field
    
    auto chat_log = Renderer([&] {
        Elements elements;
        for (const auto& msg : chat_history) {
            auto text_element = text(msg.content) | 
                (msg.role == "user" ? color(Color::Green) : color(Color::White));
            elements.push_back(text_element | vscroll_indicator | frame);
        }
        return vbox(elements) | yframe | flex | border;
    });

    auto container = Container::Vertical({
        chat_log,
        input_field,
    });

    return container | CatchEvent([&](Event event) {
        // Filter out mouse events
        if (event.is_mouse()) return false;
        
        if (event == Event::Return) {
            if (!input.empty()) {
                // Add user message
                chat_history.push_back({"user", input});
                
                // Get AI response
                try {
                    std::string response;
                    if (!fetchUrl("https://api.groq.com/openai/v1/chat/completions", response)) {
                        throw std::runtime_error("API request failed");
                    }
                    
                    auto json = nlohmann::json::parse(response);
                    std::string ai_response = json["choices"][0]["message"]["content"];
                    // Limit chat history to prevent memory issues
                    if (chat_history.size() > 50)
                        chat_history.erase(chat_history.begin());
                    chat_history.push_back({"assistant", ai_response});
                } catch (const std::exception& e) {
                    chat_history.push_back({"system", "Error: " + std::string(e.what())});
                    if (chat_history.size() > 50) chat_history.erase(chat_history.begin());
                }
                input.clear();
            }
            return true;
        }
        return false;
    });
}

int main(int argc, char* argv[]) {
    auto config = show_selection_menu();
    auto screen = ScreenInteractive::Fullscreen();
    auto chat_component = ChatInterface();
    
    // Initial system message
    chat_history.push_back({
        "system", 
        "Selected: " + config.provider + " - " + config.model + "\n"
    });
    
    screen.Loop(chat_component);
}
