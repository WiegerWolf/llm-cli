#include <iostream>
#include <string>
#include <curl/curl.h>
#include <ftxui/screen/screen.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp> // Add MenuOption header
#include <ftxui/component/screen_interactive.hpp>
#include <cstdlib> // For getenv()
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>
#include <stdexcept>

using namespace ftxui;

struct Message {
    std::string role;
    std::string content;
};

struct Config {
    std::string provider;
    std::string model;
    std::string api_base = "https://api.groq.com/openai/v1/chat/completions";
    bool needs_chat_init = true;  // Add flag to control chat initialization
    std::vector<Message> chat_history;  // Store chat history in config
};

// Callback to handle HTTP response
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t total_size = size * nmemb;
    output->append((char*)contents, total_size);
    return total_size;
}

bool fetchUrl(const std::string& url, const std::vector<Message>& chat_history, std::string& response) {
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
        nlohmann::json message_json;
        message_json["role"] = msg.role;
        message_json["content"] = msg.content;
        payload["messages"].push_back(message_json);
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

// Modify SelectionMenu to accept references to data and pointers to state
// Modify SelectionMenu to accept a pointer to selected_tab
Component SelectionMenu(Config* config,
                        const std::vector<std::string>& providers,
                        const std::vector<std::string>& models,
                        int* selected_provider,
                        int* selected_model,
                        int* selected_tab) { // Add selected_tab pointer
    // Define options for the menus. We can use on_change to update config immediately
    // or rely on the CatchEvent below for final confirmation on Enter.
    // Let's stick to the CatchEvent approach for confirming the final selection.
    MenuOption provider_option; // Basic options are often sufficient
    MenuOption model_option;

    // Use the passed-in pointers/references
    auto provider_menu = Menu(&providers, selected_provider, provider_option);
    auto model_menu = Menu(&models, selected_model, model_option);

    auto layout = Container::Vertical({
        Renderer([] { return text("Select Provider:") | bold; }),
        provider_menu,
        Renderer([] { return text("Select Model:") | bold; }),
        model_menu,
    });

    return layout | CatchEvent([&, selected_provider, selected_model](Event event) { // Capture indices by value for the lambda
        if (event == Event::Return) {
            // Update config only when Enter is pressed within this container
            // Use the dereferenced pointers to get the current selection index
            config->provider = providers[*selected_provider];
            config->model = models[*selected_model];
            // Signal that the event is handled, potentially allowing the main loop
            // CatchEvent to switch tabs.
            return true; // Event handled
        }
        return false;
    });
}

Component ChatInterface(Config* config) { // Non-const pointer
    std::string input;
    
    // Use config's initialization flag
    if (config && config->needs_chat_init && !config->provider.empty() && !config->model.empty()) {
        config->chat_history.clear();
        config->chat_history.push_back({"system", "Selected: " + config->provider + " - " + config->model});
        config->needs_chat_init = false;
    }
    
    auto input_field = Input(&input, "Type your message...") 
        | CatchEvent([&](Event event) {
            return event.is_mouse();
          }); // Disable mouse in input field
    
    auto chat_log = Renderer([&] {
        try {
            Elements elements;
            for (const auto& msg : config->chat_history) {
                auto text_element = text(msg.content) | 
                    (msg.role == "user" ? color(Color::Green) : color(Color::White));
                elements.push_back(text_element | vscroll_indicator | frame);
            }
            
            // Add empty state handling
            if (elements.empty()) {
                elements.push_back(text(" ")); // Ensure non-empty container
            }
            
            // Ensure minimum container height
            return vbox({
                text("Chat History:") | bold,
                vbox(elements) | yframe | flex
            }) | border | size(HEIGHT, GREATER_THAN, 3);
        } catch (const std::exception& e) {
            return text("Rendering error: " + std::string(e.what())) | color(Color::Red);
        }
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
                config->chat_history.push_back({"user", input});
                try {
                    std::string response;
                    bool success = fetchUrl(config->api_base, config->chat_history, response);
                    if (!success) {
                        throw std::runtime_error("API request failed");
                    }
                    
                    auto json = nlohmann::json::parse(response);
                    std::string ai_response = json["choices"][0]["message"]["content"];
                    config->chat_history.push_back({"assistant", ai_response});
                    const size_t max_history = 50;
                    if (config->chat_history.size() > max_history) {
                        config->chat_history.erase(
                            config->chat_history.begin(),
                            config->chat_history.begin() + (config->chat_history.size() - max_history)
                        );
                    }
                } catch (const std::exception& e) {
                    config->chat_history.push_back({"system", "Error: " + std::string(e.what())});
                    const size_t max_history = 50;
                    if (config->chat_history.size() > max_history) {
                        config->chat_history.erase(
                            config->chat_history.begin(),
                            config->chat_history.begin() + (config->chat_history.size() - max_history)
                        );
                    }
                }
                input.clear();
            }
            return true;
        }
        return false;
    });
}

int main(int argc, char* argv[]) {
    auto screen = ScreenInteractive::Fullscreen();
    
    Config config;
    int selected_tab = 0;

    // Declare menu data and state here in main
    std::vector<std::string> providers = {"Groq"};
    std::vector<std::string> models = {"llama-3.3-70b-versatile"};
    int selected_provider = 0;
    int selected_model = 0;

    // Pass data/state and selected_tab pointer to the factory function
    auto selection_component = SelectionMenu(&config, providers, models, &selected_provider, &selected_model, &selected_tab);
    auto chat_component = ChatInterface(&config);
    std::vector<std::string> tab_entries = {"Selection", "Chat"};
    auto tab_toggle = Toggle(&tab_entries, &selected_tab);
    auto tab_container = Container::Tab(
        {selection_component, chat_component}, // Re-enable selection_component
        // {chat_component}, // Keep only chat component for now if selection is still broken
        &selected_tab
    );
    auto main_container = Container::Vertical({
        tab_toggle,
        tab_container
    });
    auto renderer = Renderer(main_container, [&] {
        return vbox({
            tab_toggle->Render(),
            separator(),
            tab_container->Render() | flex,
        }) | border;
    }); // Remove the outer CatchEvent

    screen.Loop(renderer);
    return 0;
}
