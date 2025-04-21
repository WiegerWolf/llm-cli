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

// --- Implementation of search_web ---
std::string search_web(const std::string& query) {
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("Failed to initialize CURL for search");

    std::string response;

    char *escaped_query = curl_easy_escape(curl, query.c_str(), query.length());
    if (!escaped_query) {
        curl_easy_cleanup(curl);
        throw std::runtime_error("Failed to URL-encode search query");
    }
    std::string url = "https://search.brave.com/search?q=" + std::string(escaped_query);
    curl_free(escaped_query);

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        if (http_code == 202) {
            std::cerr << "WARNING: search_web: Received HTTP 202 Accepted. This might indicate the results page wasn't returned correctly." << std::endl;
        }
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error("Search failed: " + std::string(curl_easy_strerror(res)));
    }

    std::string parsed_result = parse_brave_search_html(response);
    return parsed_result;
}
