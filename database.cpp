#include "database.h"
#include <sqlite3.h>
#include <memory>
#include <stdexcept>
#include <iostream>
#include <cstdlib>
#include <nlohmann/json.hpp>

struct PersistenceManager::Impl {
    sqlite3* db;
    
    Impl() {
        std::string path = std::string(getenv("HOME")) + "/.llm-cli-chat.db";
        if(sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
            throw std::runtime_error("Database connection failed");
        }
        // Initialize tables
        const char* schema = R"(
            CREATE TABLE IF NOT EXISTS messages (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
                role TEXT CHECK(role IN ('system','user','assistant', 'tool')), -- Added 'tool' role
                content TEXT
            );
        )";
        exec(schema);
        exec("PRAGMA journal_mode=WAL");
    }
    
    ~Impl() { sqlite3_close(db); }

    void exec(const char* sql) {
        char* err;
        if(sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
            std::string msg = err;
            sqlite3_free(err);
            throw std::runtime_error(msg);
        }
    }

    void insertMessage(const Message& msg) {
        const char* sql = "INSERT INTO messages (role, content) VALUES (?,?)";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, msg.role.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, msg.content.c_str(), -1, SQLITE_STATIC);
        
        if(sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Insert failed");
        }
        sqlite3_finalize(stmt);
    }
};

PersistenceManager::PersistenceManager() : impl(std::make_unique<Impl>()) {}
PersistenceManager::~PersistenceManager() = default;

// Transaction Management Implementation
void PersistenceManager::beginTransaction() {
    impl->exec("BEGIN");
}

void PersistenceManager::commitTransaction() {
    impl->exec("COMMIT");
}

void PersistenceManager::rollbackTransaction() {
    impl->exec("ROLLBACK");
}


void PersistenceManager::saveUserMessage(const std::string& content) {
    // Transaction managed externally
    impl->insertMessage({"user", content});
}

void PersistenceManager::saveAssistantMessage(const std::string& content) {
    // Transaction managed externally
    impl->insertMessage({"assistant", content});
}

void PersistenceManager::saveToolMessage(const std::string& content) {
    // Note: The 'content' here is expected to be a JSON string containing
    // tool_call_id, name, and the actual tool result content.
    // The role is hardcoded as "tool".
    
    // Validate that the content is proper JSON with required fields and types
    try {
        auto json_content = nlohmann::json::parse(content);
        if (!json_content.contains("tool_call_id") || !json_content["tool_call_id"].is_string() ||
            !json_content.contains("name") || !json_content["name"].is_string() ||
            !json_content.contains("content")) { // Content can be any JSON type, but must exist
            // Throw an error if validation fails
            throw std::runtime_error("Invalid tool message content: missing required fields (tool_call_id, name, content) or incorrect types (id/name must be strings). Content: " + content);
        }
    } catch (const nlohmann::json::parse_error& e) {
        // Throw an error if JSON parsing fails
         throw std::runtime_error("Invalid tool message content: not valid JSON. Parse error: " + std::string(e.what()) + ". Content: " + content);
    } catch (const std::exception& e) {
        // Re-throw other validation errors, adding context
        throw std::runtime_error("Error validating tool message content: " + std::string(e.what()) + ". Content: " + content);
    }

    // If validation passed:
    // Transaction managed externally
    impl->insertMessage({"tool", content}); // Save the validated message
}

// Add a function to clean up orphaned tool messages
void PersistenceManager::cleanupOrphanedToolMessages() {
    // This query finds tool messages that don't have a preceding assistant message with tool_calls
    const char* sql = R"(
        DELETE FROM messages 
        WHERE role = 'tool' 
        AND id NOT IN (
            SELECT t.id FROM messages t
            JOIN messages a ON a.id < t.id
            WHERE t.role = 'tool'
            AND a.role = 'assistant'
            AND (a.content LIKE '%"tool_calls"%' OR a.content LIKE '%<function>%')
            AND (
                SELECT COUNT(*) FROM messages 
                WHERE id > a.id AND id < t.id AND role = 'assistant'
            ) = 0
        )
    )";

    // Transaction should be managed externally if needed,
    // but this cleanup is often okay as a standalone operation.
    // If called within a larger transaction, it will participate.
    // If called alone, it acts as its own transaction implicitly (autocommit).
    // For explicit control if needed elsewhere:
    // beginTransaction();
    try {
        impl->exec(sql);
        // commitTransaction(); // Only if beginTransaction was called here
    } catch (const std::exception& e) {
        // rollbackTransaction(); // Only if beginTransaction was called here
        // Re-throw or handle the error appropriately
        throw; // Re-throwing for now
    }
    // Removed explicit BEGIN/COMMIT/ROLLBACK here
}

// Removed duplicate definition of saveToolMessage

std::vector<Message> PersistenceManager::getContextHistory(size_t max_pairs) {
    // First, get the most recent system message (if any)
    const std::string system_sql = "SELECT id, role, content, timestamp FROM messages WHERE role='system' ORDER BY id DESC LIMIT 1";
    
    sqlite3_stmt* system_stmt;
    sqlite3_prepare_v2(impl->db, system_sql.c_str(), -1, &system_stmt, nullptr);
    
    std::vector<Message> history;
    if (sqlite3_step(system_stmt) == SQLITE_ROW) {
        Message system_msg;
        system_msg.id = sqlite3_column_int(system_stmt, 0);
        system_msg.role = reinterpret_cast<const char*>(sqlite3_column_text(system_stmt, 1));
        system_msg.content = reinterpret_cast<const char*>(sqlite3_column_text(system_stmt, 2));
        const unsigned char* ts = sqlite3_column_text(system_stmt, 3);
        system_msg.timestamp = ts ? reinterpret_cast<const char*>(ts) : "";
        history.push_back(system_msg);
    }
    sqlite3_finalize(system_stmt);
    
    // Now get recent messages in chronological order
    const std::string msgs_sql = R"(
        WITH recent_msgs AS (
            SELECT * FROM messages 
            WHERE role IN ('user','assistant', 'tool')
            ORDER BY id DESC 
            LIMIT ?
        )
        SELECT id, role, content, timestamp FROM recent_msgs ORDER BY id ASC
    )";

    sqlite3_stmt* msgs_stmt;
    sqlite3_prepare_v2(impl->db, msgs_sql.c_str(), -1, &msgs_stmt, nullptr);
    sqlite3_bind_int(msgs_stmt, 1, max_pairs * 2); // 2 messages per pair
    
    // Process the messages to ensure tool messages only follow tool_calls
    std::vector<Message> recent_messages;
    while(sqlite3_step(msgs_stmt) == SQLITE_ROW) {
        Message msg;
        msg.id = sqlite3_column_int(msgs_stmt, 0);
        msg.role = reinterpret_cast<const char*>(sqlite3_column_text(msgs_stmt, 1));
        msg.content = reinterpret_cast<const char*>(sqlite3_column_text(msgs_stmt, 2));
        const unsigned char* ts = sqlite3_column_text(msgs_stmt, 3);
        msg.timestamp = ts ? reinterpret_cast<const char*>(ts) : "";
        recent_messages.push_back(msg);
    }
    sqlite3_finalize(msgs_stmt);
    
    // Add recent messages to history
    history.insert(history.end(), recent_messages.begin(), recent_messages.end());
    
    // Add default system message if history is empty
    if (history.empty()) {
        history.push_back({"system", "You are a helpful assistant."});
    }
    
    return history;
}


// Updated implementation for time range query
std::vector<Message> PersistenceManager::getHistoryRange(const std::string& start_time, const std::string& end_time, size_t limit) {
    // Fetches messages within a time range, ordered chronologically, with a limit
    const char* sql = R"(
        SELECT id, role, content, timestamp FROM messages
        WHERE timestamp BETWEEN ? AND ?
        ORDER BY timestamp ASC -- Order by time
        LIMIT ?
    )";
    sqlite3_stmt* stmt = nullptr;
    std::vector<Message> history_range;

    if (sqlite3_prepare_v2(impl->db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        // Bind start_time, end_time, and limit
        sqlite3_bind_text(stmt, 1, start_time.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, end_time.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, limit);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Message msg;
            msg.id = sqlite3_column_int(stmt, 0);
            msg.role = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            msg.content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            // Retrieve and store the timestamp
            const unsigned char* ts = sqlite3_column_text(stmt, 3);
            msg.timestamp = ts ? reinterpret_cast<const char*>(ts) : ""; 
            history_range.push_back(msg);
        }
        sqlite3_finalize(stmt);
    } else {
        // std::cerr << "Warning: Failed to prepare statement for history range query: " << sqlite3_errmsg(impl->db) << std::endl; // Debug removed
        // Consider throwing an exception
    }
    return history_range;
}

