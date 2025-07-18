#include "tools_impl/read_history_tool.h"
#include <sstream>

std::string read_history(Database& db, const std::string& start_time, const std::string& end_time, size_t limit) {
    std::vector<app::db::Message> messages = db.getHistoryRange(start_time, end_time, limit);
    if (messages.empty()) {
        return "No messages found between " + start_time + " and " + end_time + " (Limit: " + std::to_string(limit) + ").";
    }

    std::stringstream ss;
    ss << "History (" << start_time << " to " << end_time << ", Limit: " << limit << "):\n";
    for (const auto& msg : messages) {
        std::string truncated_content = msg.content;
        if (truncated_content.length() > 100) {
            truncated_content = truncated_content.substr(0, 97) + "...";
        }
        size_t pos = 0;
        while ((pos = truncated_content.find('\n', pos)) != std::string::npos) {
            truncated_content.replace(pos, 1, "\\n");
            pos += 2;
        }
        ss << "[" << msg.timestamp << " ID: " << msg.id << ", Role: " << msg.role << "] " << truncated_content << "\n";
    }
    return ss.str();
}
