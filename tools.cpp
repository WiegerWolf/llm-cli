#include "tools.h"
#include "tools_impl/search_web_tool.h"
#include "tools_impl/visit_url_tool.h"
#include "tools_impl/datetime_tool.h"
#include "tools_impl/read_history_tool.h"
#include "tools_impl/web_research_tool.h"
#include "tools_impl/deep_research_tool.h"
#include "chat_client.h" // Include the full definition of ChatClient
#include "database.h" // Include database.h for PersistenceManager definition
#include "ui_interface.h" // Include UI interface
#include <stdexcept>
#include <mutex>
#include <string> // For std::to_string




// --- ToolManager Constructor (Initialize Tool Definitions) ---

ToolManager::ToolManager() :
    search_web_tool({
        {"type", "function"},
        {"function", {
            {"name", "search_web"},
            {"description", "Search the web for information using DuckDuckGo Lite. Use this for recent events, specific facts, or topics outside general knowledge."},
            {"parameters", {
                {"type", "object"},
                {"properties", {
                    {"query", {
                        {"type", "string"},
                        {"description", "The search query string."}
                    }}
                }},
                {"required", {"query"}}
            }}
        }}
    }),
    get_current_datetime_tool({
        {"type", "function"},
        {"function", {
            {"name", "get_current_datetime"},
            {"description", "Get the current date and time."},
            {"parameters", { // No parameters needed
                {"type", "object"},
                {"properties", {
                    {"format", {
                        {"type", "string"},
                        {"description", "The format of the date and time to return."},
                        {"default", "%Y-%m-%d %H:%M:%S"} // Default format
                    }}
                }},
                {"additionalProperties", false} // No additional properties allowed
            }}
        }}
    }),
    visit_url_tool({
        {"type", "function"},
        {"function", {
            {"name", "visit_url"},
            {"description", "Fetch the main text content of a given URL."},
            {"parameters", {
                {"type", "object"},
                {"properties", {
                    {"url", {
                        {"type", "string"},
                        {"description", "The full URL to visit (including http:// or https://)."}
                    }}
                }},
                {"required", {"url"}}
            }}
        }}
    }),
    read_history_tool({
        {"type", "function"},
        {"function", {
            {"name", "read_history"},
            {"description", "Read past messages from the conversation history database within a specified time range."},
            {"parameters", {
                {"type", "object"},
                {"properties", {
                    {"start_time", {
                        {"type", "string"},
                        {"description", "The start timestamp (inclusive) in 'YYYY-MM-DD HH:MM:SS' format."}
                    }},
                    {"end_time", {
                        {"type", "string"},
                        {"description", "The end timestamp (inclusive) in 'YYYY-MM-DD HH:MM:SS' format."}
                    }},
                    {"limit", {
                        {"type", "integer"},
                        {"description", "The maximum number of messages to retrieve within the range."},
                        {"default", 50} // Default limit if not specified
                    }}
                }},
                {"required", {"start_time", "end_time"}} // Require time range
            }}
        }}
    }),
    web_research_tool({ // Added definition
        {"type", "function"},
        {"function", {
            {"name", "web_research"},
            {"description", 
             "Performs multi-step web research on a given topic. This involves: "
             "1. Using 'search_web' to find relevant web pages. "
             "2. Analyzing search results and using 'visit_url' on promising links. "
             "3. Reading the content from visited pages. "
             "4. Synthesizing the gathered information into a comprehensive answer or summary for the user's original request. "
             "Use this tool when a user asks a question that requires gathering and combining information from multiple web sources."},
            {"parameters", {
                {"type", "object"},
                {"properties", {
                    {"topic", {
                        {"type", "string"},
                        {"description", "The core topic or question to research."}
                    }}
                    // Note: The LLM will need to generate the 'query' for search_web itself based on the topic.
                }},
                {"required", {"topic"}}
            }}
        }}
    }),
    deep_research_tool({ // Added definition
        {"type", "function"},
        {"function", {
            {"name", "deep_research"},
            {"description",
             "Performs in-depth research on a complex topic or goal. This tool autonomously breaks down the goal into multiple sub-topics, performs web research ('web_research' tool) for each sub-topic, and then synthesizes the findings into a comprehensive final report. Use this for broad questions requiring multi-faceted investigation beyond a single web search."},
            {"parameters", {
                {"type", "object"},
                {"properties", {
                    {"goal", {
                        {"type", "string"},
                        {"description", "The main research goal or complex question to investigate."}
                    }}
                    // Note: The tool itself will generate specific queries for internal 'web_research' calls.
                }},
                {"required", {"goal"}}
            }}
        }}
    })
{} // End Constructor


// --- ToolManager Public Methods ---

nlohmann::json ToolManager::get_tool_definitions() const {
    // Return all defined tools in a JSON array
    return nlohmann::json::array({search_web_tool, get_current_datetime_tool, visit_url_tool, read_history_tool, web_research_tool, deep_research_tool}); // Added deep_research_tool
}

// Added ChatClient& client and UserInterface& ui parameters
std::string ToolManager::execute_tool(PersistenceManager& db, ChatClient& client, UserInterface& ui, const std::string& tool_name, const nlohmann::json& args) {
    // Execute the appropriate tool based on name
    if (tool_name == "search_web") {
        std::string query = args.value("query", "");
        if (query.empty()) {
            throw std::runtime_error("'query' argument missing or empty for search_web tool.");
        }
        ui.displayStatus("[Searching web for: " + query + "]"); // Use UI for status
        try {
            return search_web(query);
        } catch (const std::exception& e) {
            return "Error performing web search: " + std::string(e.what());
        }
    } else if (tool_name == "get_current_datetime") {
        ui.displayStatus("[Getting current date and time]"); // Use UI for status
        try {
            return get_current_datetime();
        } catch (const std::exception& e) {
            return "Error getting current date and time.";
        }
    } else if (tool_name == "visit_url") {
        std::string url_to_visit = args.value("url", "");
        if (url_to_visit.empty()) {
            throw std::runtime_error("'url' argument missing or empty for visit_url tool.");
        }
        ui.displayStatus("[Visiting URL: " + url_to_visit + "]"); // Use UI for status
        try {
            return visit_url(url_to_visit);
        } catch (const std::exception& e) {
            return "Error visiting URL: " + std::string(e.what());
        }
    } else if (tool_name == "read_history") {
        std::string start_time = args.value("start_time", "");
        std::string end_time = args.value("end_time", "");
        size_t limit = args.value("limit", 50);

        if (start_time.empty() || end_time.empty()) {
            throw std::runtime_error("'start_time' or 'end_time' missing for read_history tool.");
        }
        ui.displayStatus("[Reading history (" + start_time + " to " + end_time + ", Limit: " + std::to_string(limit) + ")]"); // Use UI for status
        try {
            return read_history(db, start_time, end_time, limit);
        } catch (const std::exception& e) {
            return "Error reading history: " + std::string(e.what());
        }
    } else if (tool_name == "web_research") {
        std::string topic = args.value("topic", "");
        if (topic.empty()) {
            throw std::runtime_error("'topic' argument missing or empty for web_research tool.");
        }
        ui.displayStatus("[Performing web research on: " + topic + "]"); // Use UI for status
        return perform_web_research(db, client, ui, topic); // Pass ui
    } else if (tool_name == "deep_research") {
        std::string goal = args.value("goal", "");
        if (goal.empty()) {
            throw std::runtime_error("'goal' argument missing or empty for deep_research tool.");
        }
        ui.displayStatus("[Performing deep research for: " + goal + "]"); // Use UI for status
        return perform_deep_research(db, client, ui, goal); // Pass ui
    } else {
        throw std::runtime_error("Unknown tool requested: " + tool_name);
    }
}


