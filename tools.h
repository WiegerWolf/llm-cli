#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "database.h" // Needed for read_history tool context
#include <gumbo.h>    // Include the full Gumbo header here

// Forward declaration if needed, or include necessary headers
class PersistenceManager;

class ToolManager {
public:
    // Constructor might need PersistenceManager if tools need DB access directly
    // For now, let's pass it to execute_tool if needed.
    ToolManager(); 

    // Returns a JSON array of all tool definitions for the API call
    nlohmann::json get_tool_definitions() const;

    // Executes a tool based on its name and arguments
    // Returns the result as a string. Throws exceptions on failure.
    // Needs PersistenceManager for tools like read_history
    // Needs ChatClient for tools like web_research that need to make internal API calls
    std::string execute_tool(PersistenceManager& db, class ChatClient& client, const std::string& tool_name, const nlohmann::json& args);

private:
    // Tool definitions (moved from main.cpp)
    const nlohmann::json search_web_tool;
    const nlohmann::json get_current_datetime_tool;
    const nlohmann::json visit_url_tool;
    const nlohmann::json read_history_tool;
    const nlohmann::json web_research_tool;
    const nlohmann::json deep_research_tool; // Added declaration

    // Tool implementations (moved from main.cpp)
    std::string search_web(const std::string& query);
    std::string visit_url(const std::string& url_str);
    std::string get_current_datetime();
    // Takes PersistenceManager reference now
    std::string read_history(PersistenceManager& db, const std::string& start_time, const std::string& end_time, size_t limit);
    // Internal implementation for web research logic
    std::string perform_web_research(PersistenceManager& db, ChatClient& client, const std::string& topic);
    // Internal implementation for deep research logic
    std::string perform_deep_research(PersistenceManager& db, ChatClient& client, const std::string& goal);

    // HTML parsing helpers (moved from main.cpp)
    std::string parse_ddg_html(const std::string& html);
    std::string gumbo_get_text(GumboNode* node); // Use GumboNode directly (no struct keyword needed)
    // Static helpers can remain static or become private members
    static GumboNode* find_node_by_tag(GumboNode* node, int tag); // Use GumboNode directly
    static GumboNode* find_node_by_tag_and_class(GumboNode* node, int tag, const std::string& class_name); // Use GumboNode directly

    // CURL write callback (can be static or moved if only used here)
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output);
};
