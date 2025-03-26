#include <iostream>
#include <string>
#include <curl/curl.h>
#include <ftxui/screen/screen.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <cstdlib> // For getenv()

using namespace ftxui;

struct Config {
    std::string provider;
    std::string model;
    std::string api_base = "https://api.groq.com/openai/v1/chat/completions";
};

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
        std::cerr << "ERROR: GROQ_API_KEY environment variable not set!\n";
        return false;
    }
    std::string auth_header = "Authorization: Bearer " + std::string(api_key);
    struct curl_slist* headers = curl_slist_append(nullptr, auth_header.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "llm-cli/1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "{\"messages\": [{\"role\": \"user\", \"content\": \"Hello\"}], \"model\": \"llama-3.3-70b-versatile\"}");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, -1L); // Auto calculate length

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

int main(int argc, char* argv[]) {
    auto config = show_selection_menu();
    std::string response;
    
    // Use API endpoint
    std::string url = config.api_base;
    
    std::cout << "\nSelected: " << config.provider << " - " << config.model << std::endl;
    std::cout << "Calling: " << url << std::endl;

    if(fetchUrl(url, response)) {
        std::cout << "\nResponse (" << response.length() << " bytes):\n"
                  << response.substr(0, 500) << "\n[...truncated...]\n";
    } else {
        std::cerr << "Failed to fetch API response\n";
    }
    
    return EXIT_SUCCESS;
}
