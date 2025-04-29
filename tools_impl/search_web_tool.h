#pragma once
#include <string>

#include <nlohmann/json.hpp> // Needed for API response parsing

std::string search_web(const std::string& query);
std::string parse_brave_search_html(const std::string& html);
std::string parse_ddg_html(const std::string& html);
std::string get_brave_api_key(); // Function to get the API key
std::string call_brave_search_api(const std::string& query, const std::string& api_key); // Function to call the API
std::string parse_brave_api_response(const std::string& json_response); // Function to parse the API JSON response
