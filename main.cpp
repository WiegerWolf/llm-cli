#include <iostream>
#include <string>
#include <curl/curl.h>
#include <ftxui/screen/screen.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

using namespace ftxui;

struct Config {
    std::string provider;
    std::string model;
    std::string api_base = "https://api.groq.com/openai/v1"; // Groq base URL
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

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "llm-cli/1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    
    return res == CURLE_OK;
}

Config show_selection_menu() {
    Config config;
    auto screen = ScreenInteractive::Fullscreen();
    
    // Provider selection
    std::vector<std::string> providers = {"Groq"};
    int selected_provider = 0;
    
    auto provider_menu = Menu(&providers, &selected_provider);
    screen.Loop(Container::Vertical({
        provider_menu,
        Button("Select", [&] { screen.Exit(); })
    }));
    
    config.provider = providers[selected_provider];
    
    // Model selection based on provider
    std::vector<std::string> models = {"llama-3-70b-8192"};
    int selected_model = 0;
    
    auto model_menu = Menu(&models, &selected_model);
    screen.Loop(Container::Vertical({
        model_menu,
        Button("Select", [&] { screen.Exit(); })
    }));
    
    config.model = models[selected_model];
    return config;
}

int main(int argc, char* argv[]) {
    auto config = show_selection_menu();
    std::string response;
    
    // Construct API endpoint
    std::string url = config.api_base + "/chat/completions";
    
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
