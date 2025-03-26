#include <iostream>
#include <string>
#include <curl/curl.h>

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

int main(int argc, char* argv[]) {
    std::string name = "World";
    std::string https_response;
    
    if(argc > 1) {
        name = argv[1];
    }
    
    std::cout << "Hello " << name << "!\n";
    std::cout << "Arguments received: " << argc - 1 << std::endl << std::endl;

    // New HTTPS functionality
    if(argc > 2) {
        std::cout << "Fetching URL: " << argv[2] << "\n";
        if(fetchUrl(argv[2], https_response)) {
            std::cout << "Response (" << https_response.length() << " bytes):\n"
                      << https_response.substr(0, 500) << "\n[...truncated...]\n";
        } else {
            std::cerr << "Failed to fetch URL\n";
        }
    }
    
    return EXIT_SUCCESS;
}
