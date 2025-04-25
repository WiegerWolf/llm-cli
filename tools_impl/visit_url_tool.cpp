#include "tools_impl/visit_url_tool.h"
#include <curl/curl.h>
#include <gumbo.h>
#include <string>
#include <sstream>
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

// --- Implementation of visit_url ---
std::string visit_url(const std::string& url_str) {
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("Failed to initialize CURL");

    std::string html_content;
    long http_code = 0;

    curl_easy_setopt(curl, CURLOPT_URL, url_str.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &html_content);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "llm-cli-tool/1.0");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        return "Error fetching URL: " + std::string(curl_easy_strerror(res));
    }

    if (http_code >= 400) {
        return "Error: Received HTTP status code " + std::to_string(http_code);
    }

    GumboOutput* output = gumbo_parse(html_content.c_str());
    if (!output || !output->root) {
        if (output) gumbo_destroy_output(&kGumboDefaultOptions, output);
        return "Error: Failed to parse HTML content.";
    }

    GumboNode* body = find_node_by_tag(output->root, GUMBO_TAG_BODY);
    std::string extracted_text;

    if (body) {
        extracted_text = gumbo_get_text(body);
    } else {
        extracted_text = gumbo_get_text(output->root);
    }

    std::stringstream ss_in(extracted_text);
    std::string segment;
    std::stringstream ss_out;
    bool first = true;
    while (ss_in >> segment) {
        if (!first) ss_out << " ";
        ss_out << segment;
        first = false;
    }
    extracted_text = ss_out.str();

    gumbo_destroy_output(&kGumboDefaultOptions, output);

    std::string result = extracted_text.empty() ? "No text content found." : extracted_text;


    return result;
}
