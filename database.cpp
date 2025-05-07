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

            CREATE TABLE IF NOT EXISTS models (
                id TEXT PRIMARY KEY,
                name TEXT,
                description TEXT,
                context_length INTEGER,
                pricing_prompt TEXT,
                pricing_completion TEXT,
                architecture_input_modalities TEXT, -- JSON array of strings
                architecture_output_modalities TEXT, -- JSON array of strings
                architecture_tokenizer TEXT,
                top_provider_is_moderated INTEGER, -- Boolean (0 or 1)
                per_request_limits TEXT, -- JSON object as string
                supported_parameters TEXT, -- JSON array of strings
                created_at_api INTEGER, -- Timestamp from API 'created' field
                last_updated_db TIMESTAMP DEFAULT CURRENT_TIMESTAMP -- When this record was last updated in local DB
            );
        )";
        // Execute the schema creation statement
        exec(schema);

        // Migration: Add model_id to messages table if it doesn't exist
        bool model_id_column_exists = false;
        sqlite3_stmt* stmt_check_column = nullptr;
        std::string pragma_sql = "PRAGMA table_info('messages');";
        if (sqlite3_prepare_v2(db, pragma_sql.c_str(), -1, &stmt_check_column, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt_check_column) == SQLITE_ROW) {
                const unsigned char* col_name = sqlite3_column_text(stmt_check_column, 1);
                if (col_name && strcmp(reinterpret_cast<const char*>(col_name), "model_id") == 0) {
                    model_id_column_exists = true;
                    break;
                }
            }
        } else {
            std::string err_msg = "Failed to prepare PRAGMA table_info('messages'): ";
            err_msg += sqlite3_errmsg(db);
            sqlite3_finalize(stmt_check_column); // Finalize even on error
            throw std::runtime_error(err_msg);
        }
        sqlite3_finalize(stmt_check_column);

        if (!model_id_column_exists) {
            exec("ALTER TABLE messages ADD COLUMN model_id TEXT;");
        }

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
        const char* sql = "INSERT INTO messages (role, content, model_id) VALUES (?, ?, ?)";
        sqlite3_stmt* stmt = nullptr;
        // Prepare the SQL statement
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
             throw std::runtime_error("Failed to prepare insert statement: " + std::string(sqlite3_errmsg(db)));
        }
        // Use RAII for statement finalization
        auto stmt_guard = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>{stmt, sqlite3_finalize};

        // Bind the role and content parameters.
        sqlite3_bind_text(stmt, 1, msg.role.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, msg.content.c_str(), -1, SQLITE_STATIC);
        if (msg.model_id.has_value()) {
            sqlite3_bind_text(stmt, 3, msg.model_id.value().c_str(), -1, SQLITE_STATIC);
        } else {
            sqlite3_bind_null(stmt, 3);
        }

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
    const std::string system_sql = "SELECT id, role, content, timestamp, model_id FROM messages WHERE role='system' ORDER BY id DESC LIMIT 1";
    
    sqlite3_stmt* system_stmt;
    if (sqlite3_prepare_v2(impl->db, system_sql.c_str(), -1, &system_stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare system message query: " + std::string(sqlite3_errmsg(impl->db)));
    }
    auto system_stmt_guard = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>{system_stmt, sqlite3_finalize};

    std::vector<Message> history;
    if (sqlite3_step(system_stmt) == SQLITE_ROW) {
        Message system_msg;
        system_msg.id = sqlite3_column_int(system_stmt, 0);
        system_msg.role = reinterpret_cast<const char*>(sqlite3_column_text(system_stmt, 1));
        system_msg.content = reinterpret_cast<const char*>(sqlite3_column_text(system_stmt, 2));
        const unsigned char* ts = sqlite3_column_text(system_stmt, 3);
        system_msg.timestamp = ts ? reinterpret_cast<const char*>(ts) : "";
        const unsigned char* model_id_text = sqlite3_column_text(system_stmt, 4);
        if (model_id_text) {
            system_msg.model_id = reinterpret_cast<const char*>(model_id_text);
        }
        history.push_back(system_msg);
    }
    // system_stmt_guard will finalize automatically
    
    // Get recent user, assistant, and tool messages in chronological order.
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
    if (sqlite3_prepare_v2(impl->db, msgs_sql.c_str(), -1, &msgs_stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare recent messages query: " + std::string(sqlite3_errmsg(impl->db)));
    }
    auto msgs_stmt_guard = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>{msgs_stmt, sqlite3_finalize};
    sqlite3_bind_int(msgs_stmt, 1, max_pairs * 2);
    
    std::vector<Message> recent_messages;
    while(sqlite3_step(msgs_stmt) == SQLITE_ROW) {
        Message msg;
        msg.id = sqlite3_column_int(msgs_stmt, 0);
        msg.role = reinterpret_cast<const char*>(sqlite3_column_text(msgs_stmt, 1));
        msg.content = reinterpret_cast<const char*>(sqlite3_column_text(msgs_stmt, 2));
        const unsigned char* ts = sqlite3_column_text(msgs_stmt, 3);
        msg.timestamp = ts ? reinterpret_cast<const char*>(ts) : "";
        const unsigned char* model_id_text = sqlite3_column_text(msgs_stmt, 4);
        if (model_id_text) {
            msg.model_id = reinterpret_cast<const char*>(model_id_text);
        }
        recent_messages.push_back(msg);
    }
    // msgs_stmt_guard will finalize automatically
    
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
        SELECT id, role, content, timestamp, model_id FROM messages
        WHERE timestamp BETWEEN ? AND ?
        ORDER BY timestamp ASC
        LIMIT ?
    )";
    sqlite3_stmt* stmt = nullptr;
    std::vector<Message> history_range;

    if (sqlite3_prepare_v2(impl->db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare history range query: " + std::string(sqlite3_errmsg(impl->db)));
    }
    auto stmt_guard = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>{stmt, sqlite3_finalize};
    
    sqlite3_bind_text(stmt, 1, start_time.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, end_time.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Message msg;
        msg.id = sqlite3_column_int(stmt, 0);
        msg.role = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        msg.content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        const unsigned char* ts = sqlite3_column_text(stmt, 3);
        msg.timestamp = ts ? reinterpret_cast<const char*>(ts) : "";
        const unsigned char* model_id_text = sqlite3_column_text(stmt, 4);
        if (model_id_text) {
            msg.model_id = reinterpret_cast<const char*>(model_id_text);
        }
        history_range.push_back(msg);
    }
    // stmt_guard will finalize automatically

    return history_range;
}
// Model specific operations
void PersistenceManager::saveOrUpdateModel(const Model& model) {
    const char* sql = "INSERT OR REPLACE INTO models (id, name, description, context_length, pricing_prompt, pricing_completion, architecture_input_modalities, architecture_output_modalities, architecture_tokenizer, top_provider_is_moderated, per_request_limits, supported_parameters, created_at_api) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(impl->db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare saveOrUpdateModel statement: " + std::string(sqlite3_errmsg(impl->db)));
    }
    auto stmt_guard = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>{stmt, sqlite3_finalize};

    sqlite3_bind_text(stmt, 1, model.id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, model.name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, model.description.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, model.context_length);
    sqlite3_bind_text(stmt, 5, model.pricing_prompt.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, model.pricing_completion.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, model.architecture_input_modalities.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 8, model.architecture_output_modalities.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 9, model.architecture_tokenizer.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 10, model.top_provider_is_moderated);
    sqlite3_bind_text(stmt, 11, model.per_request_limits.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 12, model.supported_parameters.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 13, model.created_at_api);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        throw std::runtime_error("saveOrUpdateModel failed: " + std::string(sqlite3_errmsg(impl->db)));
    }
}

std::optional<Model> PersistenceManager::getModelById(const std::string& id) {
    const char* sql = "SELECT id, name, description, context_length, pricing_prompt, pricing_completion, architecture_input_modalities, architecture_output_modalities, architecture_tokenizer, top_provider_is_moderated, per_request_limits, supported_parameters, created_at_api, DATETIME(last_updated_db, 'localtime') as last_updated_db FROM models WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    std::optional<Model> result = std::nullopt;

    if (sqlite3_prepare_v2(impl->db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare getModelById statement: " + std::string(sqlite3_errmsg(impl->db)));
    }
    auto stmt_guard = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>{stmt, sqlite3_finalize};

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        Model model;
        model.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        model.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        model.description = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        model.context_length = sqlite3_column_int(stmt, 3);
        model.pricing_prompt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        model.pricing_completion = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        model.architecture_input_modalities = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        model.architecture_output_modalities = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        model.architecture_tokenizer = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
        model.top_provider_is_moderated = sqlite3_column_int(stmt, 9);
        model.per_request_limits = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));
        model.supported_parameters = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 11));
        model.created_at_api = sqlite3_column_int64(stmt, 12);
        const unsigned char* ludb = sqlite3_column_text(stmt, 13);
        model.last_updated_db = ludb ? reinterpret_cast<const char*>(ludb) : "";
        result = model;
    } else if (sqlite3_errcode(impl->db) != SQLITE_OK && sqlite3_errcode(impl->db) != SQLITE_DONE) {
        // sqlite3_step can return SQLITE_DONE if no row is found, which is not an error for "optional"
        throw std::runtime_error("getModelById failed: " + std::string(sqlite3_errmsg(impl->db)));
    }
    return result;
}

std::vector<Model> PersistenceManager::getAllModels(bool orderByName) {
    std::string sql_str = "SELECT id, name, description, context_length, pricing_prompt, pricing_completion, architecture_input_modalities, architecture_output_modalities, architecture_tokenizer, top_provider_is_moderated, per_request_limits, supported_parameters, created_at_api, DATETIME(last_updated_db, 'localtime') as last_updated_db FROM models";
    if (orderByName) {
        sql_str += " ORDER BY name ASC;";
    } else {
        sql_str += ";";
    }
    const char* sql = sql_str.c_str();
    sqlite3_stmt* stmt = nullptr;
    std::vector<Model> models;

    if (sqlite3_prepare_v2(impl->db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare getAllModels statement: " + std::string(sqlite3_errmsg(impl->db)));
    }
    auto stmt_guard = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>{stmt, sqlite3_finalize};

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Model model;
        model.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        model.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        model.description = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        model.context_length = sqlite3_column_int(stmt, 3);
        model.pricing_prompt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        model.pricing_completion = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        model.architecture_input_modalities = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        model.architecture_output_modalities = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        model.architecture_tokenizer = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
        model.top_provider_is_moderated = sqlite3_column_int(stmt, 9);
        model.per_request_limits = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));
        model.supported_parameters = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 11));
        model.created_at_api = sqlite3_column_int64(stmt, 12);
        const unsigned char* ludb = sqlite3_column_text(stmt, 13);
        model.last_updated_db = ludb ? reinterpret_cast<const char*>(ludb) : "";
        models.push_back(model);
    }
    
    if (sqlite3_errcode(impl->db) != SQLITE_OK && sqlite3_errcode(impl->db) != SQLITE_DONE) {
         throw std::runtime_error("getAllModels failed during step: " + std::string(sqlite3_errmsg(impl->db)));
    }

    return models;
}

void PersistenceManager::clearAllModels() {
    impl->exec("DELETE FROM models;");
}

// Selected model ID management
void PersistenceManager::saveSelectedModelId(const std::string& model_id) {
    impl->saveSetting("selected_model_id", model_id);
}

std::optional<std::string> PersistenceManager::loadSelectedModelId() {
    return impl->loadSetting("selected_model_id");
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
