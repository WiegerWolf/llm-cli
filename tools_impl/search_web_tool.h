#pragma once
#include <string>

// Performs a web search using the preferred method (currently DDG HTML scraping).
std::string search_web(const std::string& query);

// --- HTML Parsing Helpers (Potentially legacy/alternative methods) ---
// Parses search results from Brave Search HTML (if used).
std::string parse_brave_search_html(const std::string& html);
// Parses search results from DuckDuckGo Lite HTML (currently used by search_web).
std::string parse_ddg_html(const std::string& html);

// --- Brave Search API Helpers (Alternative method) ---
// Retrieves the Brave Search API key from environment or config.
std::string get_brave_api_key();
// Makes a call to the Brave Search API.
std::string call_brave_search_api(const std::string& query, const std::string& api_key);
// Parses the JSON response from the Brave Search API.
std::string parse_brave_api_response(const std::string& json_response);
