#include "tools_impl/search_web_tool.h"
#include <curl/curl.h>
#include <gumbo.h>
#include <string>
#include <sstream>
#include <vector>
#include <functional>
#include <stdexcept> // For runtime_error
#include <cstdlib>   // For getenv
#include <nlohmann/json.hpp> // For JSON parsing
#include <iostream> // Keep for cerr warning (static handle init failure)
#include "curl_utils.h" // Include the shared callback
#include "config.h"     // For BRAVE_SEARCH_API_KEY

// --- Gumbo helpers (static) ---
static GumboNode* find_node_by_tag(GumboNode* node, GumboTag tag) {
    if (!node || node->type != GUMBO_NODE_ELEMENT) {
        return nullptr;
    }
    if (node->v.element.tag == tag) {
        return node;
    }
    GumboVector* children = &node->v.element.children;
    for (unsigned int i = 0; i < children->length; ++i) {
        GumboNode* found = find_node_by_tag(static_cast<GumboNode*>(children->data[i]), tag);
        if (found) {
            return found;
        }
    }
    return nullptr;
}

static GumboNode* find_node_by_tag_and_class(GumboNode* node, GumboTag tag, const std::string& class_name) {
    if (!node || node->type != GUMBO_NODE_ELEMENT) {
        return nullptr;
    }
    if (node->v.element.tag == tag) {
        GumboAttribute* class_attr = gumbo_get_attribute(&node->v.element.attributes, "class");
        if (class_attr && std::string(class_attr->value).find(class_name) != std::string::npos) {
            return node;
        }
    }
    GumboVector* children = &node->v.element.children;
    for (unsigned int i = 0; i < children->length; ++i) {
        GumboNode* found = find_node_by_tag_and_class(static_cast<GumboNode*>(children->data[i]), tag, class_name);
        if (found) {
            return found;
        }
    }
    return nullptr;
}

static std::string gumbo_get_text(GumboNode* node) {
    if (!node) return "";
    if (node->type == GUMBO_NODE_TEXT) {
        return node->v.text.text;
    }
    if (node->type != GUMBO_NODE_ELEMENT) {
        return "";
    }
    if (node->v.element.tag == GUMBO_TAG_SCRIPT || node->v.element.tag == GUMBO_TAG_STYLE) {
        return "";
    }
    std::string result;
    GumboVector* children = &node->v.element.children;
    for (unsigned int i = 0; i < children->length; ++i) {
        GumboNode* child = static_cast<GumboNode*>(children->data[i]);
        result += gumbo_get_text(child);
    }
    return result;
}

// --- Constants ---
constexpr const char* kUserAgent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36";

// --- Helper function for setting up CURL for search ---
static CURL* setup_search_curl(const std::string& base_url, const std::string& query,
                               std::string& response, struct curl_slist** headers) {
    CURL* curl = curl_easy_init();
    if (!curl) return nullptr;

    char *escaped_query = curl_easy_escape(curl, query.c_str(), query.length());
    if (!escaped_query) {
        curl_easy_cleanup(curl);
        return nullptr;
    }
    // Construct the full URL
    std::string url = base_url;
    // Check if base_url already contains query parameters
    if (base_url.find('?') == std::string::npos) {
        url += "?q=";
    } else {
        url += "&q="; // Append if other parameters exist
    }
    url += escaped_query;
    curl_free(escaped_query);

    // Append User-Agent header (caller manages the list lifecycle)
    *headers = curl_slist_append(*headers, std::string("User-Agent: ").append(kUserAgent).c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, *headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

    return curl;
}


// --- Implementation of parse_brave_search_html ---
std::string parse_brave_search_html(const std::string& html) {
    GumboOutput* output = gumbo_parse(html.c_str());
    std::string result = "Web results:\n\n";
    int count = 0;

    std::vector<GumboNode*> result_divs;
    std::function<void(GumboNode*)> find_result_divs =
        [&](GumboNode* node) {
        if (!node || node->type != GUMBO_NODE_ELEMENT) return;
        if (node->v.element.tag == GUMBO_TAG_DIV) {
            GumboAttribute* class_attr = gumbo_get_attribute(&node->v.element.attributes, "class");
            if (class_attr && class_attr->value) {
                std::string classes = " " + std::string(class_attr->value) + " ";
                if (classes.find(" snippet ") != std::string::npos) {
                    result_divs.push_back(node);
                    return;
                }
            }
        }
        GumboVector* children = &node->v.element.children;
        for (unsigned int i = 0; i < children->length; ++i) {
            find_result_divs(static_cast<GumboNode*>(children->data[i]));
        }
    };

    if (output && output->root) {
        find_result_divs(output->root);

        for (GumboNode* result_div : result_divs) {
            std::string title;
            std::string url;
            std::string snippet;
            std::string display_url_text;

            GumboNode* title_a = find_node_by_tag_and_class(result_div, GUMBO_TAG_A, "heading-serpresult");
            if (title_a) {
                GumboAttribute* href = gumbo_get_attribute(&title_a->v.element.attributes, "href");
                if (href && href->value) {
                    url = href->value;
                }
                GumboNode* title_div = find_node_by_tag_and_class(title_a, GUMBO_TAG_DIV, "title");
                if (title_div) {
                    title = gumbo_get_text(title_div);
                } else {
                    title = gumbo_get_text(title_a);
                }
                title.erase(0, title.find_first_not_of(" \n\r\t"));
                title.erase(title.find_last_not_of(" \n\r\t") + 1);
            }

            GumboNode* snippet_div = find_node_by_tag_and_class(result_div, GUMBO_TAG_DIV, "snippet-description");
            if (snippet_div) {
                snippet = gumbo_get_text(snippet_div);
                snippet.erase(0, snippet.find_first_not_of(" \n\r\t"));
                snippet.erase(snippet.find_last_not_of(" \n\r\t") + 1);
            } else {
                GumboNode* snippet_content_div = find_node_by_tag_and_class(result_div, GUMBO_TAG_DIV, "snippet-content");
                if (snippet_content_div) {
                    snippet = gumbo_get_text(snippet_content_div);
                    snippet.erase(0, snippet.find_first_not_of(" \n\r\t"));
                    snippet.erase(snippet.find_last_not_of(" \n\r\t") + 1);
                }
            }

            GumboNode* url_cite = find_node_by_tag_and_class(result_div, GUMBO_TAG_CITE, "snippet-url");
            if (url_cite) {
                display_url_text = gumbo_get_text(url_cite);
                display_url_text.erase(0, display_url_text.find_first_not_of(" \n\r\t"));
                display_url_text.erase(display_url_text.find_last_not_of(" \n\r\t") + 1);
            } else {
                GumboNode* url_div = find_node_by_tag_and_class(result_div, GUMBO_TAG_DIV, "url");
                if (url_div) {
                    display_url_text = gumbo_get_text(url_div);
                    display_url_text.erase(0, display_url_text.find_first_not_of(" \n\r\t"));
                    display_url_text.erase(display_url_text.find_last_not_of(" \n\r\t") + 1);
                }
            }

            if (!title.empty() && !url.empty()) {
                result += std::to_string(++count) + ". " + title + "\n";
                if (!snippet.empty()) {
                    result += "   " + snippet + "\n";
                }
                std::string display_url = display_url_text.empty() ? url : display_url_text;
                result += "   " + display_url + " [href=" + url + "]\n\n";
            }
        }
        gumbo_destroy_output(&kGumboDefaultOptions, output);
    }

    std::string final_result = count > 0 ? result : "No results found or failed to parse results page.";
    return final_result;
}


// --- Implementation of parse_ddg_html (adapted from user provided code) ---
// Parses the html.duckduckgo.com/html/ structure
// Note: This function uses a static CURL handle for URL decoding to improve performance.
// The handle is intentionally never cleaned up as it exists for the lifetime of the application.
std::string parse_ddg_html(const std::string& html) {
    GumboOutput* output = gumbo_parse(html.c_str());
    std::string result = "Web results (from DuckDuckGo):\n\n"; // Indicate source
    int count = 0;

    std::vector<GumboNode*> result_divs;
    std::function<void(GumboNode*)> find_result_divs =
        [&](GumboNode* node) {
        if (!node || node->type != GUMBO_NODE_ELEMENT) return;

        // Check if the node is a DIV with class containing "result"
        if (node->v.element.tag == GUMBO_TAG_DIV) {
            GumboAttribute* class_attr = gumbo_get_attribute(&node->v.element.attributes, "class");
            // Check if class exists and contains "result" as a word
            if (class_attr && class_attr->value) {
                 std::string classes = " " + std::string(class_attr->value) + " ";
                 // DDG HTML uses "result" class for the main container
                 if (classes.find(" result ") != std::string::npos && classes.find(" result--ad ") == std::string::npos) { // Exclude ads
                     result_divs.push_back(node);
                     // Don't recurse further into a result div
                     return;
                 }
            }
        }

        // Recurse into children if not a result div
        GumboVector* children = &node->v.element.children;
        for (unsigned int i = 0; i < children->length; ++i) {
            find_result_divs(static_cast<GumboNode*>(children->data[i]));
        }
    };

    if (output && output->root) {
        find_result_divs(output->root);

        for (GumboNode* result_div : result_divs) {
            std::string title;
            std::string url;
            std::string snippet;
            std::string display_url_text; // The visible URL text

            // Find Title and URL (usually within h2 > a.result__a)
            GumboNode* title_h2 = find_node_by_tag(result_div, GUMBO_TAG_H2);
            GumboNode* title_a = title_h2 ? find_node_by_tag_and_class(title_h2, GUMBO_TAG_A, "result__a") : nullptr;

            if (title_a) {
                GumboAttribute* href = gumbo_get_attribute(&title_a->v.element.attributes, "href");
                if (href && href->value) {
                    url = href->value;
                    // DDG often uses redirection links, try to decode them
                    // Example: /l/?kh=-1&uddg=https%3A%2F%2Fwww.example.com
                    size_t uddg_pos = url.find("uddg=");
                    if (uddg_pos != std::string::npos) {
                        std::string encoded_url = url.substr(uddg_pos + 5); // Length of "uddg="
                        // Use a static handle for efficiency, initialized only once.
                        // Note: This handle is intentionally not cleaned up; it persists for the application's lifetime.
                        // For a CLI tool, this is generally acceptable.
                        static CURL* unescape_handle = curl_easy_init();
                        if (unescape_handle) {
                            int outlength;
                            char* decoded = curl_easy_unescape(unescape_handle, encoded_url.c_str(), encoded_url.length(), &outlength);
                            if (decoded) {
                                url = std::string(decoded, outlength);
                                curl_free(decoded);
                            }
                            // Do not call curl_easy_cleanup here; the handle is static.
                        } else {
                            // Handle error: static handle initialization failed (should be rare)
                            std::cerr << "Warning: Failed to initialize static CURL handle for URL unescaping." << std::endl;
                        }
                    }
                }
                title = gumbo_get_text(title_a);
                title.erase(0, title.find_first_not_of(" \n\r\t"));
                title.erase(title.find_last_not_of(" \n\r\t") + 1);
            }

            // Find Snippet (usually within a.result__snippet)
            GumboNode* snippet_a = find_node_by_tag_and_class(result_div, GUMBO_TAG_A, "result__snippet");
            if (snippet_a) {
                snippet = gumbo_get_text(snippet_a);
                snippet.erase(0, snippet.find_first_not_of(" \n\r\t"));
                snippet.erase(snippet.find_last_not_of(" \n\r\t") + 1);
            }

            // Find Display URL (usually within a.result__url)
            GumboNode* url_a = find_node_by_tag_and_class(result_div, GUMBO_TAG_A, "result__url");
            if (url_a) {
                display_url_text = gumbo_get_text(url_a);
                display_url_text.erase(0, display_url_text.find_first_not_of(" \n\r\t"));
                display_url_text.erase(display_url_text.find_last_not_of(" \n\r\t") + 1);
            }

            // Add result if we found the essentials (title and actual URL)
            if (!title.empty() && !url.empty()) {
                result += std::to_string(++count) + ". " + title + "\n";
                if (!snippet.empty()) {
                    result += "   " + snippet + "\n";
                }
                // Use the display URL text if available, otherwise fallback to the actual URL
                std::string display_url = display_url_text.empty() ? url : display_url_text;
                result += "   " + display_url + " [href=" + url + "]\n\n";
            }
        } // End loop through result divs

        gumbo_destroy_output(&kGumboDefaultOptions, output);
    }

    std::string final_result = count > 0 ? result : "No results found or failed to parse results page (DuckDuckGo)."; // Indicate source on failure too
    return final_result;
}


// --- Brave Search API Helpers ---

// Function to get the Brave Search API key (similar to OpenRouter key retrieval)
std::string get_brave_api_key() {
    constexpr const char* compiled_key = BRAVE_SEARCH_API_KEY;
    // Check if the key was set at compile time and is not the placeholder or the CMake default "OFF"
    if (compiled_key[0] != '\0' &&
        std::string(compiled_key) != "@BRAVE_SEARCH_API_KEY@" &&
        std::string(compiled_key) != "OFF") { // CMake option() defaults to OFF
        return std::string(compiled_key);
    }
    // If not compiled, check the environment variable
    const char* env_key = std::getenv("BRAVE_SEARCH_API_KEY");
    if (env_key) {
        return std::string(env_key);
    }
    // If neither is set, return an empty string to indicate failure
    return "";
}

// Function to call the Brave Search API
std::string call_brave_search_api(const std::string& query, const std::string& api_key) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL for Brave API search");
    }

    std::string response_string;
    std::string url = "https://api.search.brave.com/res/v1/web/search";
    char *escaped_query = curl_easy_escape(curl, query.c_str(), query.length());
    if (!escaped_query) {
        curl_easy_cleanup(curl);
        throw std::runtime_error("Failed to escape query for Brave API search");
    }
    url += "?q=" + std::string(escaped_query);
    curl_free(escaped_query);

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Accept: application/json");
    // Note: Accept-Encoding is handled by CURLOPT_ACCEPT_ENCODING below
    headers = curl_slist_append(headers, ("X-Subscription-Token: " + api_key).c_str());
    // Add User-Agent (optional but good practice)
    headers = curl_slist_append(headers, std::string("User-Agent: ").append(kUserAgent).c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // Follow redirects
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L); // Slightly longer timeout for API call
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    // Enable automatic decompression if gzip is used by the server
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "gzip");

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error("Brave API request failed: " + std::string(curl_easy_strerror(res)));
    }

    if (http_code < 200 || http_code >= 300) {
        throw std::runtime_error("Brave API request failed with HTTP code " + std::to_string(http_code) + ". Response: " + response_string);
    }

    return response_string;
}

// Function to parse the Brave Search API JSON response
std::string parse_brave_api_response(const std::string& json_response) {
    try {
        nlohmann::json response_json = nlohmann::json::parse(json_response);
        std::string result = "Web results (from Brave API):\n\n";
        int count = 0;

        // Check if the 'web' key and 'results' array exist
        if (response_json.contains("web") && response_json["web"].is_object() &&
            response_json["web"].contains("results") && response_json["web"]["results"].is_array())
        {
            const size_t max_results = 10; // Limit the number of results
            for (const auto& item : response_json["web"]["results"]) {
                if (count >= max_results) break; // Stop after reaching the limit
                if (!item.is_object()) continue;

                std::string title = item.value("title", "");
                std::string url = item.value("url", "");
                std::string description = item.value("description", ""); // Snippet/description
                std::string display_url_text; // Brave API might provide profile.display_url

                // Try to get a display URL if available (e.g., from profile)
                if (item.contains("profile") && item["profile"].is_object() && item["profile"].contains("display_url")) {
                    display_url_text = item["profile"]["display_url"].get<std::string>();
                }

                if (!title.empty() && !url.empty()) {
                    result += std::to_string(++count) + ". " + title + "\n";
                    if (!description.empty()) {
                        result += "   " + description + "\n";
                    }
                    std::string display_url = display_url_text.empty() ? url : display_url_text;
                    result += "   " + display_url + " [href=" + url + "]\n\n";
                }
            }
        }

        if (count > 0) {
            return result;
        } else {
            return "No web results found in Brave API response.";
        }

    } catch (const nlohmann::json::parse_error& e) {
        // std::cerr << "Error parsing Brave API JSON response: " << e.what() << std::endl; // Debug removed
        return "Error parsing Brave API JSON response.";
    } catch (const std::exception& e) {
        return "Error processing Brave API response.";
    }
}


// --- Implementation of search_web ---
std::string search_web(const std::string& query) {
    CURL* curl = nullptr; // Initialize later for HTML scraping
    CURLcode res;
    std::string response;
    std::string parsed_result;
    long http_code = 0;
    std::string brave_html_error_reason = "Unknown"; // Store failure reason
    std::string ddg_html_error_reason = "Unknown";   // Store failure reason

    // --- Attempt 1: Brave Search ---
    struct curl_slist* brave_headers = nullptr;
    curl = setup_search_curl("https://search.brave.com/search", query, response, &brave_headers);
    if (!curl) {
        // Cleanup headers if allocated by helper before failure
        if (brave_headers) curl_slist_free_all(brave_headers);
        throw std::runtime_error("Failed to setup CURL for Brave search");
    }

    res = curl_easy_perform(curl);
    http_code = 0; // Reset http_code before checking
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        if (http_code == 202) {
            // WARNING: Brave Search: Received HTTP 202 Accepted.
        }
    }

    // Cleanup Brave resources
    curl_slist_free_all(brave_headers);
    curl_easy_cleanup(curl);
    curl = nullptr; // Reset curl handle for safety

    if (res == CURLE_OK && http_code >= 200 && http_code < 300) {
        parsed_result = parse_brave_search_html(response);
        // Check if Brave search returned actual results (contains links)
        if (parsed_result.find("[href=") != std::string::npos) {
            // Brave Search successful.
            return parsed_result; // Return Brave results
        } else {
            // Brave Search returned no results, falling back to DuckDuckGo...
            brave_html_error_reason = "No results found or parse failed."; // Capture reason
        }
    } else {
        // Brave Search failed, falling back to DuckDuckGo...
        if (res != CURLE_OK) {
            brave_html_error_reason = "CURL error: " + std::string(curl_easy_strerror(res));
        } else {
            brave_html_error_reason = "HTTP error: " + std::to_string(http_code);
        }
    }

    // --- Attempt 2: DuckDuckGo HTML Search (Fallback) ---
    response.clear(); // Clear previous response
    struct curl_slist* ddg_headers = nullptr;
    // Note: DDG URL needs extra params, so we pass the base URL with them
    curl = setup_search_curl("https://html.duckduckgo.com/html/?kl=us-en", query, response, &ddg_headers);
    if (!curl) {
        // Cleanup headers if allocated by helper before failure
        if (ddg_headers) curl_slist_free_all(ddg_headers);
        // If Brave failed, this means both failed.
        throw std::runtime_error("Failed to setup CURL for DDG search (after Brave failure)");
    }

    // std::cerr << "DEBUG: DDG Search: Requesting URL (GET): " << /* Need to reconstruct URL if debugging */ << std::endl; // Debug removed

    res = curl_easy_perform(curl);
    http_code = 0; // Reset http_code before checking
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        if (http_code == 202) {
             // WARNING: DDG Search: Received HTTP 202 Accepted.
        }
    }

    // Cleanup DDG resources
    curl_slist_free_all(ddg_headers);
    curl_easy_cleanup(curl);
    curl = nullptr; // Reset curl handle for safety

    if (res != CURLE_OK) {
        // DDG Search also failed
        // Capture DDG failure reason before checking API key
        ddg_html_error_reason = "CURL error: " + std::string(curl_easy_strerror(res));
        // Fall through to API attempt
    } else if (http_code < 200 || http_code >= 300) {
        // Capture HTTP error for DDG
        ddg_html_error_reason = "HTTP error: " + std::to_string(http_code);
        // Fall through to API attempt
    } else {
        // Parse DDG results if request was successful
        parsed_result = parse_ddg_html(response);
        // Check if DDG search returned actual results (contains links)
        if (parsed_result.find("[href=") != std::string::npos) {
            // DuckDuckGo Search successful.
            return parsed_result; // Return DDG results
        } else {
            // DuckDuckGo Search returned no results, falling back to Brave API...
            ddg_html_error_reason = "No results found or parse failed."; // Capture reason
        }
    }


    // --- Attempt 3: Brave Search API (Final Fallback) ---
    std::string brave_api_key = get_brave_api_key();
    if (brave_api_key.empty()) {
        // Brave API key not found. Cannot use API fallback.
        // Return the last result we got (which was the "no results" message from DDG parser)
        return parsed_result;
    }

    try {
        // Attempting Brave Search API call...
        std::string api_response_json = call_brave_search_api(query, brave_api_key);
        parsed_result = parse_brave_api_response(api_response_json);
        // Brave Search API call finished.
        // The parser function handles "no results found" internally
        return parsed_result;
    } catch (const std::exception& e) {
        // Brave Search API failed.
        // Return the detailed error message including reasons for each failure
        return "All search methods failed. Brave HTML: " + brave_html_error_reason +
               ", DDG HTML: " + ddg_html_error_reason +
               ", Brave API: " + std::string(e.what());
    }
}
