#include "message_repository.h"
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace database {

MessageRepository::MessageRepository(DatabaseCore& core)
    : core_(core), current_session_id_(1) {
}

void MessageRepository::setCurrentSession(int session_id) {
    current_session_id_ = session_id;
}

int MessageRepository::getCurrentSession() const {
    return current_session_id_;
}

void MessageRepository::insertUserMessage(const std::string& content) {
    Message msg;
    msg.role = "user";
    msg.content = content;
    msg.model_id = std::nullopt;
    insertMessage(msg);
}

void MessageRepository::insertAssistantMessage(const std::string& content, const std::string& model_id) {
    Message msg;
    msg.role = "assistant";
    msg.content = content;
    msg.model_id = model_id.empty() ? std::nullopt : std::make_optional(model_id);
    insertMessage(msg);
}

void MessageRepository::insertToolMessage(const std::string& content) {
    // Validate tool message format before insertion
    validateToolMessage(content);
    
    Message msg;
    msg.role = "tool";
    msg.content = content;
    msg.model_id = std::nullopt;
    insertMessage(msg);
}

std::vector<Message> MessageRepository::getContextHistory(size_t max_pairs) {
    // First, get the most recent system message for this session
    const std::string system_sql = "SELECT id, role, content, timestamp, model_id FROM messages WHERE role='system' AND session_id=? ORDER BY id DESC LIMIT 1";

    auto system_stmt = core_.prepareStatement(system_sql);
    sqlite3_bind_int(system_stmt.get(), 1, current_session_id_);

    std::vector<Message> history;
    if (sqlite3_step(system_stmt.get()) == SQLITE_ROW) {
        Message system_msg;
        system_msg.id = sqlite3_column_int(system_stmt.get(), 0);
        system_msg.role = reinterpret_cast<const char*>(sqlite3_column_text(system_stmt.get(), 1));
        system_msg.content = reinterpret_cast<const char*>(sqlite3_column_text(system_stmt.get(), 2));
        const unsigned char* ts = sqlite3_column_text(system_stmt.get(), 3);
        system_msg.timestamp = ts ? reinterpret_cast<const char*>(ts) : "";

        if (sqlite3_column_type(system_stmt.get(), 4) != SQLITE_NULL) {
            system_msg.model_id = reinterpret_cast<const char*>(sqlite3_column_text(system_stmt.get(), 4));
        } else {
            system_msg.model_id = std::nullopt;
        }
        history.push_back(system_msg);
    }

    // Get recent user/assistant/tool messages for this session
    const std::string msgs_sql = R"(
        WITH recent_msgs AS (
            SELECT id, role, content, timestamp, model_id FROM messages
            WHERE role IN ('user', 'assistant', 'tool') AND session_id=?
            ORDER BY id DESC
            LIMIT ?
        )
        SELECT id, role, content, timestamp, model_id FROM recent_msgs ORDER BY id ASC
    )";

    auto msgs_stmt = core_.prepareStatement(msgs_sql);
    sqlite3_bind_int(msgs_stmt.get(), 1, current_session_id_);
    sqlite3_bind_int(msgs_stmt.get(), 2, static_cast<int>(max_pairs * 2));

    std::vector<Message> recent_messages;
    while(sqlite3_step(msgs_stmt.get()) == SQLITE_ROW) {
        Message msg;
        msg.id = sqlite3_column_int(msgs_stmt.get(), 0);
        msg.role = reinterpret_cast<const char*>(sqlite3_column_text(msgs_stmt.get(), 1));
        msg.content = reinterpret_cast<const char*>(sqlite3_column_text(msgs_stmt.get(), 2));
        const unsigned char* ts = sqlite3_column_text(msgs_stmt.get(), 3);
        msg.timestamp = ts ? reinterpret_cast<const char*>(ts) : "";

        if (sqlite3_column_type(msgs_stmt.get(), 4) != SQLITE_NULL) {
            msg.model_id = reinterpret_cast<const char*>(sqlite3_column_text(msgs_stmt.get(), 4));
        } else {
            msg.model_id = std::nullopt;
        }
        recent_messages.push_back(msg);
    }

    history.insert(history.end(), recent_messages.begin(), recent_messages.end());

    // If no messages exist, add a default system message
    if (history.empty()) {
        Message default_system_msg;
        default_system_msg.role = "system";
        default_system_msg.content = "You are a helpful assistant.";
        default_system_msg.id = 0;
        default_system_msg.timestamp = "";
        default_system_msg.model_id = std::nullopt;
        history.push_back(default_system_msg);
    }

    return history;
}

std::vector<Message> MessageRepository::getHistoryRange(const std::string& start_time,
                                                         const std::string& end_time,
                                                         size_t limit) {
    const char* sql = R"(
        SELECT id, role, content, timestamp, model_id FROM messages
        WHERE timestamp BETWEEN ? AND ?
        ORDER BY timestamp ASC
        LIMIT ?
    )";
    
    auto stmt = core_.prepareStatement(sql);
    
    sqlite3_bind_text(stmt.get(), 1, start_time.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt.get(), 2, end_time.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt.get(), 3, static_cast<int>(limit));

    std::vector<Message> history_range;
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        Message msg;
        msg.id = sqlite3_column_int(stmt.get(), 0);
        msg.role = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));
        msg.content = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 2));
        const unsigned char* ts = sqlite3_column_text(stmt.get(), 3);
        msg.timestamp = ts ? reinterpret_cast<const char*>(ts) : "";
        
        if (sqlite3_column_type(stmt.get(), 4) != SQLITE_NULL) {
            msg.model_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 4));
        } else {
            msg.model_id = std::nullopt;
        }
        history_range.push_back(msg);
    }
    
    return history_range;
}

void MessageRepository::cleanupOrphanedToolMessages() {
    const char* sql = R"(
        DELETE FROM messages
        WHERE role = 'tool'
        AND id NOT IN (
            SELECT t.id
            FROM messages t
            JOIN messages a ON a.id < t.id
            WHERE t.role = 'tool'
              AND a.role = 'assistant'
              AND (a.content LIKE '%"tool_calls"%' OR a.content LIKE '%<function>%')
              AND (
                  SELECT COUNT(*)
                  FROM messages intervening_a
                  WHERE intervening_a.id > a.id AND intervening_a.id < t.id AND intervening_a.role = 'assistant'
              ) = 0
        )
    )";
    
    try {
        core_.exec(sql);
    } catch (const std::exception& e) {
        throw;
    }
}

void MessageRepository::insertMessage(const Message& msg) {
    const char* sql = "INSERT INTO messages (role, content, model_id, session_id) VALUES (?, ?, ?, ?)";

    auto stmt = core_.prepareStatement(sql);

    sqlite3_bind_text(stmt.get(), 1, msg.role.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt.get(), 2, msg.content.c_str(), -1, SQLITE_STATIC);

    if (msg.model_id.has_value()) {
        sqlite3_bind_text(stmt.get(), 3, msg.model_id.value().c_str(), -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_null(stmt.get(), 3);
    }

    sqlite3_bind_int(stmt.get(), 4, current_session_id_);

    if(sqlite3_step(stmt.get()) != SQLITE_DONE) {
        throw std::runtime_error("Insert failed: " + std::string(sqlite3_errmsg(core_.getConnection())));
    }
}

void MessageRepository::validateToolMessage(const std::string& content) {
    try {
        auto json_content = nlohmann::json::parse(content);
        if (!json_content.contains("tool_call_id") || !json_content["tool_call_id"].is_string() ||
            !json_content.contains("name") || !json_content["name"].is_string() ||
            !json_content.contains("content")) {
            throw std::runtime_error("Invalid tool message content: missing required fields (tool_call_id, name, content) or incorrect types (id/name must be strings). Content: " + content);
        }
    } catch (const nlohmann::json::parse_error& e) {
         throw std::runtime_error("Invalid tool message content: not valid JSON. Parse error: " + std::string(e.what()) + ". Content: " + content);
    } catch (const std::exception& e) {
        throw std::runtime_error("Error validating tool message content: " + std::string(e.what()) + ". Content: " + content);
    }
}

Message MessageRepository::buildMessageFromRow(sqlite3_stmt* stmt) {
    Message msg;
    msg.id = sqlite3_column_int(stmt, 0);
    msg.role = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    msg.content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    const unsigned char* ts = sqlite3_column_text(stmt, 3);
    msg.timestamp = ts ? reinterpret_cast<const char*>(ts) : "";
    
    if (sqlite3_column_type(stmt, 4) != SQLITE_NULL) {
        msg.model_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    } else {
        msg.model_id = std::nullopt;
    }
    
    return msg;
}

} // namespace database