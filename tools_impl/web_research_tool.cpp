#include "tools_impl/web_research_tool.h"
#include "tools_impl/search_web_tool.h"
#include "tools_impl/visit_url_tool.h"
#include "chat_client.h"
#include "ui_interface.h" // Include UI interface
#include <sstream>
#include <vector>
#include <future>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string> // For std::to_string

std::string perform_web_research(PersistenceManager& db, ChatClient& client, UserInterface& ui, const std::string& topic) {
    try {
        ui.displayStatus("  [Research Step 1: Searching web...]"); // Use UI for status
        std::string search_query = topic;
        std::string search_results_raw = search_web(search_query);

        std::vector<std::string> urls;
        std::stringstream ss_search(search_results_raw);
        std::string line;
        while (getline(ss_search, line)) {
            size_t href_start = line.find(" [href=");
            if (href_start != std::string::npos && line.find("   ") == 0) {
                size_t url_start_pos = href_start + 7;
                size_t url_end_pos = line.find(']', url_start_pos);
                if (url_end_pos != std::string::npos) {
                    std::string extracted_url = line.substr(url_start_pos, url_end_pos - url_start_pos);
                    if (extracted_url.rfind("http", 0) == 0 || extracted_url.rfind("https", 0) == 0) {
                        urls.push_back(extracted_url);
                    } else {
#ifdef VERBOSE_LOGGING
                        ui.displayError("[web_research] skipped non-absolute URL: " + extracted_url); // Use UI for verbose error
#endif
                    }
                }
            }
        }
        ui.displayStatus("  [Research Step 2: Found " + std::to_string(urls.size()) + " absolute URLs. Visiting all...]"); // Use UI for status

        std::string visited_content_summary = "\n\nVisited Pages Content:\n";
        std::mutex summary_mutex;

        if (urls.empty()) {
            visited_content_summary += "No relevant URLs found in search results to visit.\n";
        } else {
            std::vector<std::future<std::pair<std::string, std::string>>> futures;

            for (const std::string& url : urls) {
                futures.push_back(std::async(std::launch::async, [url]() {
                    try {
                        std::string content = visit_url(url);
                        return std::make_pair(url, content);
                    } catch (const std::exception& e) {
                        return std::make_pair(url, "Error visiting URL: " + std::string(e.what()));
                    }
                }));
            }

            ui.displayStatus("  [Research Step 3: Waiting for URL visits to complete...]"); // Use UI for status
            for (size_t i = 0; i < futures.size(); ++i) {
                try {
                    std::pair<std::string, std::string> result = futures[i].get();
                    const std::string& url = result.first;
                    const std::string& content_or_error = result.second;

                    std::lock_guard<std::mutex> lock(summary_mutex);
                    if (content_or_error.rfind("Error visiting URL:", 0) == 0) {
                        visited_content_summary += "\n--- Failed to visit " + url + ": " + content_or_error.substr(20) + " ---\n";
                    } else {
                        visited_content_summary += "\n--- Content from " + url + " ---\n";
                        visited_content_summary += content_or_error;
                        visited_content_summary += "\n--- End Content ---\n";
                    }
                } catch (const std::exception& e) {
                    std::lock_guard<std::mutex> lock(summary_mutex);
                    visited_content_summary += "\n--- Error retrieving result from future: " + std::string(e.what()) + " ---\n";
                }
            }
        }

        ui.displayStatus("  [Research Step 4: Synthesizing results...]"); // Use UI for status
        std::string synthesis_context = "Web search results for '" + topic + "':\n" + search_results_raw + visited_content_summary;

        std::vector<Message> synthesis_messages;
        synthesis_messages.push_back({"system", "You are a research assistant. Based *only* on the provided text which contains web search results and content from visited web pages, synthesize a comprehensive answer to the original research topic. Do not add any preamble like 'Based on the provided text...'."});
        synthesis_messages.push_back({"user", "Original research topic: " + topic + "\n\nProvided research context:\n" + synthesis_context});

        synthesis_messages[0].content = "You are a research assistant. Based *only* on the provided text which contains web search results and content from visited web pages, synthesize a comprehensive answer to the original research topic. DO NOT USE ANY TOOLS OR FUNCTIONS. Do not add any preamble like 'Based on the provided text...'";

        std::string final_synthesized_content;
        bool synthesis_success = false;

        for (int attempt = 0; attempt < 3 && !synthesis_success; attempt++) {
            if (attempt > 0) {
                synthesis_messages[0].content = "CRITICAL INSTRUCTION: You are a research assistant. Your ONLY task is to write a plain text summary based on the provided research. DO NOT USE ANY TOOLS OR FUNCTIONS WHATSOEVER. DO NOT INCLUDE ANY <function> TAGS OR TOOL CALLS. Just write normal text.";
            }

            std::string synthesis_response_str = client.makeApiCall(synthesis_messages, false);
            nlohmann::json synthesis_response_json;
            try {
                synthesis_response_json = nlohmann::json::parse(synthesis_response_str);
            } catch (const nlohmann::json::parse_error& e) {
                // JSON Parsing Error (Synthesis Response)
                if (attempt == 2) return "Error: Failed to parse synthesis response from LLM.";
                continue;
            }

            if (synthesis_response_json.contains("choices") &&
                !synthesis_response_json["choices"].empty() &&
                synthesis_response_json["choices"][0].contains("message")) {

                auto message = synthesis_response_json["choices"][0]["message"];

                if (message.contains("tool_calls") && !message["tool_calls"].is_null()) {
                    // Warning: Synthesis response contains tool_calls. Retrying with stronger instructions.
                    continue;
                }

                if (message.contains("content") && message["content"].is_string()) {
                    final_synthesized_content = message["content"];
                    synthesis_success = true;
                    break;
                }
            }

            if (attempt == 2) {
                // Error: Invalid API response structure (Synthesis Response).
                return "Error: Invalid response structure from LLM during synthesis.";
            }
        }

        if (!synthesis_success) {
            return "I researched information about '" + topic + "' but encountered technical difficulties synthesizing the results. The search found relevant information, but I was unable to properly summarize it due to API limitations.";
        }

        ui.displayStatus("[Web research complete for: " + topic + "]"); // Use UI for status
        return final_synthesized_content;

    } catch (const std::exception& e) {
        ui.displayError("Web research failed during execution: " + std::string(e.what())); // Use UI for error
        return "Error performing web research: " + std::string(e.what());
    }
}
