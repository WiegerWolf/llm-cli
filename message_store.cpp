#include "message_store.h"
#include "sqlite_connection.h"
#include <sqlite3.h>
#include <memory>
#include <stdexcept>
#include <nlohmann/json.hpp>

namespace app {
namespace db {
namespace {
struct SQLiteStmtDeleter {
    void operator()(sqlite3_stmt* stmt) const {
        if (stmt) {
            sqlite3_finalize(stmt);
        }
    }
};
using unique_sqlite_stmt_ptr = std::unique_ptr<sqlite3_stmt, SQLiteStmtDeleter>;
} // end anonymous namespace

MessageStore::MessageStore(SQLiteConnection& db_conn) : m_db_conn(db_conn) {}
void detail::MessageStoreDetail::insertMessage(SQLiteConnection& db_conn, const Message& msg) {
    const char* sql = "INSERT INTO messages (role, content, model_id) VALUES (?, ?, ?)";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_conn.getDbHandle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
         throw std::runtime_error("Failed to prepare insert statement: " + std::string(sqlite3_errmsg(db_conn.getDbHandle())));
    }
    unique_sqlite_stmt_ptr stmt_guard(stmt);
    sqlite3_bind_text(stmt, 1, msg.role.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, msg.content.c_str(), -1, SQLITE_STATIC);
    if (msg.model_id.has_value()) {
        sqlite3_bind_text(stmt, 3, msg.model_id.value().c_str(), -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_null(stmt, 3);
    }
    if(sqlite3_step(stmt) != SQLITE_DONE) {
        throw std::runtime_error("Insert failed: " + std::string(sqlite3_errmsg(db_conn.getDbHandle())));
    }
}
void MessageStore::saveUserMessage(const std::string& content) {
    Message msg;
    msg.role = "user";
    msg.content = content;
    msg.model_id = std::nullopt;
    detail::MessageStoreDetail::insertMessage(m_db_conn, msg);
}
void MessageStore::saveAssistantMessage(const std::string& content, const std::string& model_id) {
    Message msg;
    msg.role = "assistant";
    msg.content = content;
    msg.model_id = model_id.empty() ? std::nullopt : std::make_optional(model_id);
    detail::MessageStoreDetail::insertMessage(m_db_conn, msg);
}
void MessageStore::saveToolMessage(const std::string& content) {
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
    Message msg;
    msg.role = "tool";
    msg.content = content;
    msg.model_id = std::nullopt;
    detail::MessageStoreDetail::insertMessage(m_db_conn, msg);
}
void MessageStore::cleanupOrphanedToolMessages() {
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
    m_db_conn.exec(sql);
}
std::vector<Message> MessageStore::getContextHistory(size_t max_pairs) {
    const std::string system_sql = "SELECT id, role, content, timestamp, model_id FROM messages WHERE role='system' ORDER BY id DESC LIMIT 1";
    sqlite3_stmt* system_stmt;
    if (sqlite3_prepare_v2(m_db_conn.getDbHandle(), system_sql.c_str(), -1, &system_stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare system message query: " + std::string(sqlite3_errmsg(m_db_conn.getDbHandle())));
    }
    unique_sqlite_stmt_ptr system_stmt_guard(system_stmt);
    std::vector<Message> history;
    if (sqlite3_step(system_stmt) == SQLITE_ROW) {
        Message system_msg;
        system_msg.id = sqlite3_column_int(system_stmt, 0);
        system_msg.role = reinterpret_cast<const char*>(sqlite3_column_text(system_stmt, 1));
        system_msg.content = reinterpret_cast<const char*>(sqlite3_column_text(system_stmt, 2));
        const unsigned char* ts = sqlite3_column_text(system_stmt, 3);
        system_msg.timestamp = ts ? reinterpret_cast<const char*>(ts) : "";
        if (sqlite3_column_type(system_stmt, 4) != SQLITE_NULL) {
            system_msg.model_id = reinterpret_cast<const char*>(sqlite3_column_text(system_stmt, 4));
        } else {
            system_msg.model_id = std::nullopt;
        }
        history.push_back(system_msg);
    }
    
    const std::string msgs_sql = R"(
        WITH recent_msgs AS (
            SELECT id, role, content, timestamp, model_id FROM messages
            WHERE role IN ('user', 'assistant', 'tool')
            ORDER BY id DESC
            LIMIT ?
        )
        SELECT id, role, content, timestamp, model_id FROM recent_msgs ORDER BY id ASC
    )";
    sqlite3_stmt* msgs_stmt;
    if (sqlite3_prepare_v2(m_db_conn.getDbHandle(), msgs_sql.c_str(), -1, &msgs_stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare recent messages query: " + std::string(sqlite3_errmsg(m_db_conn.getDbHandle())));
    }
    unique_sqlite_stmt_ptr msgs_stmt_guard(msgs_stmt);
    sqlite3_bind_int(msgs_stmt, 1, static_cast<int>(max_pairs * 2));
    
    std::vector<Message> recent_messages;
    while(sqlite3_step(msgs_stmt) == SQLITE_ROW) {
        Message msg;
        msg.id = sqlite3_column_int(msgs_stmt, 0);
        msg.role = reinterpret_cast<const char*>(sqlite3_column_text(msgs_stmt, 1));
        msg.content = reinterpret_cast<const char*>(sqlite3_column_text(msgs_stmt, 2));
        const unsigned char* ts = sqlite3_column_text(msgs_stmt, 3);
        msg.timestamp = ts ? reinterpret_cast<const char*>(ts) : "";
        if (sqlite3_column_type(msgs_stmt, 4) != SQLITE_NULL) {
            msg.model_id = reinterpret_cast<const char*>(sqlite3_column_text(msgs_stmt, 4));
        } else {
            msg.model_id = std::nullopt;
        }
        recent_messages.push_back(msg);
    }
    
    history.insert(history.end(), recent_messages.begin(), recent_messages.end());
    
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
std::vector<Message> MessageStore::getHistoryRange(const std::string& start_time, const std::string& end_time, size_t limit) {
    const char* sql = R"(
        SELECT id, role, content, timestamp, model_id FROM messages
        WHERE timestamp BETWEEN ? AND ?
        ORDER BY timestamp ASC
        LIMIT ?
    )";
    sqlite3_stmt* stmt = nullptr;
    std::vector<Message> history_range;
    if (sqlite3_prepare_v2(m_db_conn.getDbHandle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare history range query: " + std::string(sqlite3_errmsg(m_db_conn.getDbHandle())));
    }
    unique_sqlite_stmt_ptr stmt_guard(stmt);
    
    sqlite3_bind_text(stmt, 1, start_time.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, end_time.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, static_cast<int>(limit));
    while (sqlite3_step(stmt) == SQLITE_ROW) {
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
        history_range.push_back(msg);
    }
    return history_range;
}
} // namespace db
} // namespace app