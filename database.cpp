#include "database.h"
#include <sqlite3.h>
#include <memory>
#include <stdexcept>
#include <iostream>
#include <cstdlib>
#include <nlohmann/json.hpp>

struct PersistenceManager::Impl {
    sqlite3* db;
    
    // Constructor for the implementation class
    Impl() : db(nullptr) { // Initialize db pointer
        // Construct the database path in the user's home directory
        const char* home_dir = std::getenv("HOME");
        if (!home_dir) {
            throw std::runtime_error("Failed to get HOME directory environment variable.");
        }
        std::string path = std::string(home_dir) + "/.llm-cli-chat.db";

        // Open the SQLite database connection
        if(sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
            std::string err_msg = "Database connection failed: ";
            if (db) { // sqlite3_open sets db even on error
                err_msg += sqlite3_errmsg(db);
                sqlite3_close(db); // Close the handle if opened partially
                db = nullptr;
            } else {
                err_msg += "Could not allocate memory for database handle.";
            }
            throw std::runtime_error(err_msg);
        }

        // Define the database schema
        const char* schema = R"(
            CREATE TABLE IF NOT EXISTS messages (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp DATETIME DEFAULT CURRENT_TIMESTAMP, -- Automatically record insertion time
                role TEXT CHECK(role IN ('system','user','assistant', 'tool')), -- Ensure valid roles
                content TEXT -- Store message content (can be plain text or JSON string for tool/assistant)
            );

            CREATE TABLE IF NOT EXISTS settings (
                key TEXT PRIMARY KEY NOT NULL,
                value TEXT
            );
        )";
        // Execute the schema creation statement
        exec(schema);
        // Enable Write-Ahead Logging (WAL) mode for better concurrency
        exec("PRAGMA journal_mode=WAL");
    }
    
    // Destructor for the implementation class
    ~Impl() {
        if (db) {
            sqlite3_close(db); // Ensure database connection is closed
        }
    }

    // Executes a simple SQL statement without expecting results.
    // Throws std::runtime_error on failure.
    void exec(const char* sql) {
        char* err = nullptr;
        if(sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
            std::string msg = "SQL error executing '";
            msg += sql;
            msg += "': ";
            msg += err;
            sqlite3_free(err); // Free the error message allocated by SQLite
            throw std::runtime_error(msg);
        }
    }

    // Inserts a message into the database using a prepared statement.
    void insertMessage(const Message& msg) {
        const char* sql = "INSERT INTO messages (role, content) VALUES (?, ?)";
        sqlite3_stmt* stmt = nullptr;
        // Prepare the SQL statement
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
             throw std::runtime_error("Failed to prepare insert statement: " + std::string(sqlite3_errmsg(db)));
        }
        // Use RAII for statement finalization
        auto stmt_guard = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>{stmt, sqlite3_finalize};

        // Bind the role and content parameters.
        // SQLITE_STATIC means SQLite doesn't need to copy the data, assuming it remains valid until step.
        sqlite3_bind_text(stmt, 1, msg.role.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, msg.content.c_str(), -1, SQLITE_STATIC);

        // Execute the prepared statement.
        if(sqlite3_step(stmt) != SQLITE_DONE) {
            // stmt_guard will finalize the statement automatically
            throw std::runtime_error("Insert failed: " + std::string(sqlite3_errmsg(db)));
        }
        // Statement finalized automatically by stmt_guard
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
    // This query deletes 'tool' role messages that are considered "orphaned".
    // An orphaned tool message is one that does not have a valid preceding 'assistant' message
    // which requested tool execution (either via standard 'tool_calls' or fallback '<function>' tags).
    // It ensures that we don't keep tool results whose corresponding request was lost or invalid.
    const char* sql = R"(
        DELETE FROM messages
        WHERE role = 'tool' -- Target only tool messages for deletion
        AND id NOT IN ( -- Keep tool messages whose ID *is* in the result of this subquery
            SELECT t.id -- Select the IDs of valid tool messages
            FROM messages t -- Alias the tool message table as 't'
            JOIN messages a ON a.id < t.id -- Join with a preceding message 'a'
            WHERE t.role = 'tool' -- Ensure 't' is a tool message
              AND a.role = 'assistant' -- Ensure 'a' is an assistant message
              -- Check if the assistant message 'a' contains either standard tool calls or fallback function tags
              AND (a.content LIKE '%"tool_calls"%' OR a.content LIKE '%<function>%')
              -- Ensure there are NO other assistant messages between 'a' and 't'
              -- This prevents matching a tool message to an old assistant request if a newer assistant message exists in between.
              AND (
                  SELECT COUNT(*)
                  FROM messages intervening_a
                  WHERE intervening_a.id > a.id AND intervening_a.id < t.id AND intervening_a.role = 'assistant'
              ) = 0
            -- Note: This doesn't strictly validate the JSON or the specific tool_call_id match here,
            -- it assumes that if an assistant message requested *any* tool, and a tool message
            -- immediately follows (without another assistant in between), it's likely valid.
            -- Stricter validation happens during context reconstruction in makeApiCall.
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
    
    // Get recent user, assistant, and tool messages in chronological order.
    // Note: This query fetches the last N messages based on ID. It does *not*
    // perform the strict validation of assistant->tool sequences here; that logic
    // is handled during context reconstruction in ChatClient::makeApiCall.
    // This function primarily retrieves a candidate set of recent messages.
    const std::string msgs_sql = R"(
        -- Use a Common Table Expression (CTE) for clarity
        WITH recent_msgs AS (
            -- Select all columns from messages
            SELECT * FROM messages
            -- Filter for relevant roles
            WHERE role IN ('user', 'assistant', 'tool')
            -- Order by ID descending to get the most recent ones
            ORDER BY id DESC
            -- Limit the number of messages retrieved (max_pairs * 2, e.g., 10 pairs = 20 messages)
            LIMIT ?
        )
        -- Select the final columns from the CTE, ordered chronologically (ascending ID)
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
        std::string error_msg = "Failed to prepare statement for history range query: ";
        error_msg += sqlite3_errmsg(impl->db);
        sqlite3_finalize(stmt); // Ensure statement is finalized even on prepare error
        throw std::runtime_error(error_msg);
    }
    return history_range;
}


// Settings Management Implementation
void PersistenceManager::saveSetting(const std::string& key, const std::string& value) {
    const char* sql = "INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(impl->db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare saveSetting statement: " + std::string(sqlite3_errmsg(impl->db)));
    }
    // Use RAII for statement finalization
    auto stmt_guard = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>{stmt, sqlite3_finalize};

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, value.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        // stmt_guard will finalize the statement automatically
        throw std::runtime_error("saveSetting failed: " + std::string(sqlite3_errmsg(impl->db)));
    }
    // Statement finalized automatically by stmt_guard
}

std::optional<std::string> PersistenceManager::loadSetting(const std::string& key) {
    const char* sql = "SELECT value FROM settings WHERE key = ?";
    sqlite3_stmt* stmt = nullptr;
    std::optional<std::string> result = std::nullopt;

    if (sqlite3_prepare_v2(impl->db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare loadSetting statement: " + std::string(sqlite3_errmsg(impl->db)));
    }
    // Use RAII for statement finalization
    auto stmt_guard = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>{stmt, sqlite3_finalize};

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC);

    int step_result = sqlite3_step(stmt);
    if (step_result == SQLITE_ROW) {
        const unsigned char* text = sqlite3_column_text(stmt, 0);
        if (text) {
            result = reinterpret_cast<const char*>(text);
        }
    } else if (step_result != SQLITE_DONE) {
        // stmt_guard will finalize the statement automatically
        throw std::runtime_error("loadSetting failed: " + std::string(sqlite3_errmsg(impl->db)));
    }
    // Statement finalized automatically by stmt_guard

    return result;
}
