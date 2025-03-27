#include <iostream>
#include <string>
#include <curl/curl.h>
#include <ftxui/screen/screen.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <cstdlib> // For getenv()
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>
#include <stdexcept>

// Add this operator overload to handle Event formatting
std::ostream& operator<<(std::ostream& os, const ftxui::Event& event) {
  if (event.is_character()) {
    os << "Char:" << event.character();
  } else if (event == ftxui::Event::Return) {
    os << "Enter";
  } else if (event == ftxui::Event::Escape) {
    os << "Esc";
  } else if (event.is_mouse()) {
    os << "Mouse";
  } else {
    os << "UnknownEvent";
  }
  return os;
}

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

    std::cerr << "Building payload with " << chat_history.size() << " messages\n";
    std::cerr << "Payload size: " << json_payload.size() << " bytes\n";

    CURLcode res = curl_easy_perform(curl);
    
    std::cerr << "Response code: " << res << "\n";
    std::cerr << "Response size: " << response.size() << " bytes\n";
    if (!response.empty()) {
        std::cerr << "First 200 chars of response:\n" << response.substr(0, 200) << "\n";
    }
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    return res == CURLE_OK;
}

Component SelectionMenu(Config* config) {
    std::cerr << "\nCreating selection menu\n";
    std::vector<std::string> providers = {"Groq"};
    std::vector<std::string> models = {"llama-3.3-70b-versatile"};
    int selected_provider = 0;
    int selected_model = 0;

    auto provider_menu = Menu(&providers, &selected_provider);
    auto model_menu = Menu(&models, &selected_model);

    return Container::Vertical({
        provider_menu,
        model_menu,
    }) | CatchEvent([&](Event event) {
        std::cerr << "Selection menu event: " << event << "\n";
        if (event == Event::Return) {
            std::cerr << "Setting config (provider: " << providers[selected_provider]
                     << ", model: " << models[selected_model] << ")\n";
            config->provider = providers[selected_provider];
            config->model = models[selected_model];
            return true; // Exit the component
        }
        return false;
    });
}

Component ChatInterface(const Config* config) { // Change to pointer
    std::string input;
    static std::vector<Message> chat_history;  // Make history local to component
    static bool needs_init = true;  // Track initialization status
    
    auto input_field = Input(&input, "Type your message...") 
        | CatchEvent([&](Event event) {
            return event.is_mouse();
          }); // Disable mouse in input field
    
    auto chat_log = Renderer([&] {
        try {
            std::cerr << "\nRendering chat log (needs_init: " << std::boolalpha << needs_init
                     << ", config valid: " << (config != nullptr)
                     << ")\n";
            
            // Add null check for config pointer
            if (config && needs_init && !config->provider.empty() && !config->model.empty()) {
                std::cerr << "Initializing system message\n";
                chat_history.push_back({"system", "Selected: " + config->provider + " - " + config->model});
                needs_init = false;
            }
            
            std::cerr << "Chat history size: " << chat_history.size() << "\n";
            Elements elements;
            for (const auto& msg : chat_history) {
                auto text_element = text(msg.content) | 
                    (msg.role == "user" ? color(Color::Green) : color(Color::White));
                elements.push_back(text_element | vscroll_indicator | frame);
            }
            
            // Add empty state handling
            if (elements.empty()) {
                std::cerr << "Rendering empty chat state\n";
                elements.push_back(text(" ")); // Ensure non-empty container
            }
            
            std::cerr << "Returning rendered elements\n";
            // Ensure minimum container height
            return vbox({
                text("Chat History:") | bold,
                vbox(elements) | yframe | flex
            }) | border | size(HEIGHT, GREATER_THAN, 3);
        } catch (const std::exception& e) {
            std::cerr << "Rendering error: " << e.what() << "\n";
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
                std::cerr << "--- New Message ---\n";
                std::cerr << "Current history size: " << chat_history.size() << "\n";
                
                // Add user message
                chat_history.push_back({"user", input});
                std::cerr << "After user message: " << chat_history.size() << "\n";
                
                // Get AI response
                try {
                    std::string response;
                    bool success = fetchUrl(config->api_base, chat_history, response);
                    std::cerr << "API call success: " << std::boolalpha << success << "\n";
                    
                    if (!success) {
                        throw std::runtime_error("API request failed");
                    }
                    
                    auto json = nlohmann::json::parse(response);
                    std::string ai_response = json["choices"][0]["message"]["content"];
                    
                    std::cerr << "Before assistant message: " << chat_history.size() << "\n";
                    chat_history.push_back({"assistant", ai_response});
                    std::cerr << "After assistant message: " << chat_history.size() << "\n";
                    
                    // Keep last 50 messages (25 exchanges)
                    const size_t max_history = 50;
                    if (chat_history.size() > max_history) {
                        std::cerr << "Trimming history from " << chat_history.size() 
                                << " to " << max_history << "\n";
                        chat_history.erase(
                            chat_history.begin(),
                            chat_history.begin() + (chat_history.size() - max_history)
                        );
                        std::cerr << "New history size: " << chat_history.size() << "\n";
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Exception caught: " << e.what() << "\n";
                    chat_history.push_back({"system", "Error: " + std::string(e.what())});
                    
                    // Keep last 50 messages (25 exchanges)
                    const size_t max_history = 50;
                    if (chat_history.size() > max_history) {
                        std::cerr << "Error trimming history from " << chat_history.size() 
                                << " to " << max_history << "\n";
                        chat_history.erase(
                            chat_history.begin(),
                            chat_history.begin() + (chat_history.size() - max_history)
                        );
                    }
                    
                    std::cerr << "Current history size after error: " << chat_history.size() << "\n";
                }
                
                std::cerr << "Final history size: " << chat_history.size() << "\n";
                input.clear();
            }
            return true;
        }
        return false;
    });
}

int main(int argc, char* argv[]) {
    std::cerr << "\n=== Program Start ===\n";
    auto screen = ScreenInteractive::Fullscreen();
    std::cerr << "Screen initialized\n";
    
    Config config;
    std::cerr << "Config created (provider: '" << config.provider 
             << "', model: '" << config.model << "')\n";

    int selected_tab = 0;
    std::cerr << "Selected tab initialized to 0\n";

    auto selection_component = SelectionMenu(&config);
    std::cerr << "Selection component created\n";

    auto chat_component = ChatInterface(&config);
    std::cerr << "Chat component created\n";

    std::cerr << "Creating main container...\n";
    auto main_container = Container::Tab({
        selection_component,
        chat_component,
    }, &selected_tab) | CatchEvent([&](Event event) {
        std::cerr << "Main container event: " << event << "\n";
        if (event == Event::Return && selected_tab == 0) {
            std::cerr << "Switching to chat tab\n";
            selected_tab = 1;
            return true;
        }
        return false;
    });

    std::cerr << "Entering screen loop...\n";
    screen.Loop(main_container);
    std::cerr << "Exiting program\n";
    return 0;
}
