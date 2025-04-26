#pragma once
#include <string>

std::string search_web(const std::string& query);
std::string parse_brave_search_html(const std::string& html);
std::string parse_ddg_html(const std::string& html); // Add declaration for DDG parser
