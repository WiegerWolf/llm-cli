#include "tools_impl/search_web_tool.h"
#include <curl/curl.h>
#include <gumbo.h>
#include <string>
#include <sstream>
#include <vector>
#include <functional>
#include <iostream>

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

// --- CURL WriteCallback (static) ---
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t total_size = size * nmemb;
    output->append((char*)contents, total_size);
    return total_size;
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
        // std::cerr << "DEBUG: parse_ddg_html: Found " << result_divs.size() << " potential result divs." << std::endl; // Debug removed

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
                        CURL* temp_curl = curl_easy_init(); // Need curl for URL decoding
                        if (temp_curl) {
                            int outlength;
                            char* decoded = curl_easy_unescape(temp_curl, encoded_url.c_str(), encoded_url.length(), &outlength);
                            if (decoded) {
                                url = std::string(decoded, outlength);
                                curl_free(decoded);
                            }
                            curl_easy_cleanup(temp_curl);
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
    } else {
         std::cerr << "DEBUG: parse_ddg_html: Gumbo output or root node was null." << std::endl;
    }

    std::string final_result = count > 0 ? result : "No results found or failed to parse results page (DuckDuckGo)."; // Indicate source on failure too
    return final_result;
}


// --- Implementation of search_web ---
std::string search_web(const std::string& query) {
    CURL* curl = nullptr; // Initialize later
    CURLcode res;
    std::string response;
    std::string parsed_result;
    long http_code = 0;

    // --- Attempt 1: Brave Search ---
    std::cerr << "Attempting search with Brave Search..." << std::endl;
    curl = curl_easy_init();
    if (!curl) throw std::runtime_error("Failed to initialize CURL for Brave search");

    // Removed redeclaration of response here

    char *escaped_query = curl_easy_escape(curl, query.c_str(), query.length());
    if (!escaped_query) {
        curl_easy_cleanup(curl);
        throw std::runtime_error("Failed to URL-encode search query");
    }
    std::string brave_url = "https://search.brave.com/search?q=" + std::string(escaped_query);
    curl_free(escaped_query);

    struct curl_slist* brave_headers = nullptr;
    brave_headers = curl_slist_append(brave_headers, "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36");

    curl_easy_setopt(curl, CURLOPT_URL, brave_url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, brave_headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

    res = curl_easy_perform(curl);
    http_code = 0;
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        std::cerr << "DEBUG: Brave Search HTTP status code: " << http_code << std::endl;
        if (http_code == 202) {
            std::cerr << "WARNING: Brave Search: Received HTTP 202 Accepted." << std::endl;
        }
    }

    curl_slist_free_all(brave_headers);
    curl_easy_cleanup(curl);
    curl = nullptr; // Reset curl handle

    if (res == CURLE_OK && http_code >= 200 && http_code < 300) {
        parsed_result = parse_brave_search_html(response);
        // Check if Brave search returned actual results (contains links)
        if (parsed_result.find("[href=") != std::string::npos) {
            std::cerr << "Brave Search successful." << std::endl;
            return parsed_result; // Return Brave results
        } else {
            std::cerr << "Brave Search returned no results, falling back to DuckDuckGo..." << std::endl;
        }
    } else {
        std::cerr << "Brave Search failed (CURL error: " << curl_easy_strerror(res)
                  << ", HTTP code: " << http_code << "), falling back to DuckDuckGo..." << std::endl;
    }

    // --- Attempt 2: DuckDuckGo HTML Search (Fallback) ---
    response.clear(); // Clear previous response
    curl = curl_easy_init();
    if (!curl) throw std::runtime_error("Failed to initialize CURL for DDG search");

    escaped_query = curl_easy_escape(curl, query.c_str(), query.length());
    if (!escaped_query) {
        curl_easy_cleanup(curl);
        throw std::runtime_error("Failed to URL-encode search query for DDG");
    }
    std::string ddg_url = "https://html.duckduckgo.com/html/?q=" + std::string(escaped_query) + "&kl=us-en"; // Add region parameter
    curl_free(escaped_query);

    struct curl_slist* ddg_headers = nullptr;
    ddg_headers = curl_slist_append(ddg_headers, "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36");

    curl_easy_setopt(curl, CURLOPT_URL, ddg_url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, ddg_headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

    std::cerr << "DEBUG: DDG Search: Requesting URL (GET): " << ddg_url << std::endl;

    res = curl_easy_perform(curl);
    http_code = 0;
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        std::cerr << "DEBUG: DDG Search: Received HTTP status code: " << http_code << std::endl;
        if (http_code == 202) {
             std::cerr << "WARNING: DDG Search: Received HTTP 202 Accepted." << std::endl;
        }
    }

    curl_slist_free_all(ddg_headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        std::cerr << "DDG Search also failed: " << curl_easy_strerror(res) << std::endl;
        // Return the error message from the *first* failure (Brave) or a generic one if Brave didn't even run
        // For simplicity, let's throw a new error indicating both failed.
        throw std::runtime_error("Both Brave and DuckDuckGo search attempts failed.");
    }

    // Parse DDG results regardless of HTTP code for now, parser handles "no results"
    parsed_result = parse_ddg_html(response);
    std::cerr << "DuckDuckGo Search finished." << std::endl;
    return parsed_result; // Return DDG results (could be "no results found")
}
