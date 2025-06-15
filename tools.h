#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "database.h" // Needed for read_history tool context
#include <gumbo.h>    // Include the full Gumbo header here

// Forward declaration if needed, or include necessary headers
class Database;
class UserInterface; // Forward declaration

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
    // Needs UserInterface to display status messages
    std::string execute_tool(Database& db, class ChatClient& client, UserInterface& ui, const std::string& tool_name, const nlohmann::json& args);

private:
    // Tool definitions (JSON schemas for the API)
    // These are initialized in the ToolManager constructor (in tools.cpp)
    // and aggregated by get_tool_definitions().
    nlohmann::json search_web_tool;
    nlohmann::json get_current_datetime_tool;
    nlohmann::json visit_url_tool;
    nlohmann::json read_history_tool;
    nlohmann::json web_research_tool;
    nlohmann::json deep_research_tool; // Added declaration
};

// Tool implementations (free functions)
#include "tools_impl/search_web_tool.h"
#include "tools_impl/visit_url_tool.h"
#include "tools_impl/datetime_tool.h"
#include "tools_impl/read_history_tool.h"
#include "tools_impl/web_research_tool.h"
#include "tools_impl/deep_research_tool.h"
