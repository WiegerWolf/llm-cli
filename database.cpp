#include "database.h"
#include <sqlite3.h>
#include <memory>
#include <stdexcept>
#include <iostream>
#include <cstdlib>
#include <nlohmann/json.hpp>

// Constructor for the implementation class
PersistenceManager::Impl::Impl() : db(nullptr) { // Initialize db pointer
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

    // Define the database schema (original version, migration will add model_id to messages)
    const char* schema = R"(
        CREATE TABLE IF NOT EXISTS messages (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            role TEXT CHECK(role IN ('system','user','assistant', 'tool')),
            content TEXT,
            model_id TEXT -- <-- New column
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
            architecture_input_modalities TEXT, 
            architecture_output_modalities TEXT, 
            architecture_tokenizer TEXT,
            top_provider_is_moderated INTEGER, 
            per_request_limits TEXT, 
            supported_parameters TEXT, 
            created_at_api INTEGER, 
            last_updated_db TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );
    )";
    // Execute the schema creation statement
    this->exec(schema);

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
        if (stmt_check_column) sqlite3_finalize(stmt_check_column);
        throw std::runtime_error(err_msg);
    }
    sqlite3_finalize(stmt_check_column);

    if (!model_id_column_exists) {
        this->exec("ALTER TABLE messages ADD COLUMN model_id TEXT;");
    }

    // Enable Write-Ahead Logging (WAL) mode for better concurrency
    this->exec("PRAGMA journal_mode=WAL");
}

// Destructor for the implementation class
PersistenceManager::Impl::~Impl() {
    if (db) {
        sqlite3_close(db); // Ensure database connection is closed
    }
}

// Executes a simple SQL statement without expecting results. (const char* version)
void PersistenceManager::Impl::exec(const char* sql) {
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

// Executes a simple SQL statement without expecting results. (const std::string& version)
void PersistenceManager::Impl::exec(const std::string& sql) {
    char* errMsg = nullptr;
    if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string error = "SQL error in exec (std::string): " + std::string(errMsg);
        sqlite3_free(errMsg);
        throw std::runtime_error(error);
    }
}

// Inserts a message into the database using a prepared statement.
void PersistenceManager::Impl::insertMessage(const Message& msg) {
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
    // Bind model_id
    if (!msg.model_id.empty()) {
        sqlite3_bind_text(stmt, 3, msg.model_id.c_str(), -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_null(stmt, 3);
    }

    // Execute the prepared statement.
    if(sqlite3_step(stmt) != SQLITE_DONE) {
        throw std::runtime_error("Insert failed: " + std::string(sqlite3_errmsg(db)));
    }
}

// Settings management for Impl
void PersistenceManager::Impl::saveSetting(const std::string& key, const std::string& value) {
    const char* sql = "INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare Impl::saveSetting statement: " + std::string(sqlite3_errmsg(db)));
    }
    auto stmt_guard = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>{stmt, sqlite3_finalize};

    if (sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC) != SQLITE_OK) {
        throw std::runtime_error("Failed to bind key in Impl::saveSetting: " + std::string(sqlite3_errmsg(db)));
    }
    if (sqlite3_bind_text(stmt, 2, value.c_str(), -1, SQLITE_STATIC) != SQLITE_OK) {
        throw std::runtime_error("Failed to bind value in Impl::saveSetting: " + std::string(sqlite3_errmsg(db)));
    }

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        throw std::runtime_error("Impl::saveSetting failed: " + std::string(sqlite3_errmsg(db)));
    }
}

std::optional<std::string> PersistenceManager::Impl::loadSetting(const std::string& key) {
    const char* sql = "SELECT value FROM settings WHERE key = ?";
    sqlite3_stmt* stmt = nullptr;
    std::optional<std::string> result = std::nullopt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (stmt) sqlite3_finalize(stmt);
        return std::nullopt; 
    }
    auto stmt_guard = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>{stmt, sqlite3_finalize};

    if (sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC) != SQLITE_OK) {
        return std::nullopt;
    }

    int step_result = sqlite3_step(stmt);
    if (step_result == SQLITE_ROW) {
        const unsigned char* text = sqlite3_column_text(stmt, 0);
        if (text) {
            result = reinterpret_cast<const char*>(text);
        }
    } else if (step_result != SQLITE_DONE) {
        return std::nullopt;
    }
    return result;
}

// PersistenceManager methods
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
    Message msg;
    msg.role = "user";
    msg.content = content;
    msg.model_id = ""; // Empty model_id for user messages
    impl->insertMessage(msg);
}

void PersistenceManager::saveAssistantMessage(const std::string& content, const std::string& model_id) {
    Message msg;
    msg.role = "assistant";
    msg.content = content;
    msg.model_id = model_id;
    impl->insertMessage(msg);
}

void PersistenceManager::saveToolMessage(const std::string& content) {
    try {
        auto json_content = nlohmann::json::parse(content);
        if (!json_content.contains("tool_call_id") || !json_content["tool_call_id"].is_string() ||
            !json_content.contains("name") || !json_content["name"].is_string() ||
            !json_content.contains("content")) {
            throw std::runtime_error("Invalid tool message content: missing required fields (tool_call_id, name, content) or incorrect types (id/name must be strings). Content: " + content);
        }
    } catch (const nlohmann::json::parse_error& e) {
         throw std::runtime_error("Invalid tool message content: not valid JSON. Parse error: " + std::string(e.what()) + ". Content: " + content);
    } catch (const std::exception& e) { // Catch other std::exceptions from validation logic
        throw std::runtime_error("Error validating tool message content: " + std::string(e.what()) + ". Content: " + content);
    }
    Message msg;
    msg.role = "tool";
    msg.content = content;
    msg.model_id = ""; // Empty model_id for tool messages
    impl->insertMessage(msg);
}

void PersistenceManager::cleanupOrphanedToolMessages() {
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
        impl->exec(sql);
    } catch (const std::exception& e) {
        throw;
    }
}

std::vector<Message> PersistenceManager::getContextHistory(size_t max_pairs) {
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
        } else {
            system_msg.model_id = ""; // Assign empty string if null
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
    if (sqlite3_prepare_v2(impl->db, msgs_sql.c_str(), -1, &msgs_stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare recent messages query: " + std::string(sqlite3_errmsg(impl->db)));
    }
    auto msgs_stmt_guard = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>{msgs_stmt, sqlite3_finalize};
    sqlite3_bind_int(msgs_stmt, 1, static_cast<int>(max_pairs * 2)); 
    
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
        } else {
            msg.model_id = ""; // Assign empty string if null
        }
        recent_messages.push_back(msg);
    }
    
    history.insert(history.end(), recent_messages.begin(), recent_messages.end());
    
    if (history.empty()) {
        Message default_system_msg;
        default_system_msg.role = "system";
        default_system_msg.content = "You are a helpful assistant.";
        default_system_msg.id = 0; // Or some other appropriate default or leave to DB
        default_system_msg.timestamp = ""; // Or a current timestamp
        default_system_msg.model_id = ""; // Assign empty string
        history.push_back(default_system_msg);
    }
    
    return history;
}

std::vector<Message> PersistenceManager::getHistoryRange(const std::string& start_time, const std::string& end_time, size_t limit) {
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
    sqlite3_bind_int(stmt, 3, static_cast<int>(limit));

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
        } else {
            msg.model_id = ""; // Assign empty string if null
        }
        history_range.push_back(msg);
    }
    return history_range;
}

/*
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

void PersistenceManager::clearAllModels() { // Renamed to clearModelsTable
    impl->exec("DELETE FROM models;");
}
*/

// New model methods using ModelData
void PersistenceManager::clearModelsTable() {
    impl->exec("DELETE FROM models;");
}

void PersistenceManager::insertOrUpdateModel(const ModelData& model) {
    // Using only id and name as per current ModelData definition
    const char* sql = "INSERT INTO models (id, name) VALUES (?, ?) ON CONFLICT(id) DO UPDATE SET name=excluded.name;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(impl->db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare insertOrUpdateModel statement: " + std::string(sqlite3_errmsg(impl->db)));
    }
    auto stmt_guard = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>{stmt, sqlite3_finalize};

    sqlite3_bind_text(stmt, 1, model.id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, model.name.c_str(), -1, SQLITE_STATIC);
    // If ModelData is expanded, bind other parameters here. E.g.:
    // sqlite3_bind_int(stmt, 3, model.context_length);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        throw std::runtime_error("insertOrUpdateModel failed: " + std::string(sqlite3_errmsg(impl->db)));
    }
}

std::vector<ModelData> PersistenceManager::getAllModels() {
    // Using only id and name as per current ModelData definition
    const char* sql = "SELECT id, name FROM models ORDER BY name ASC;"; // Added ORDER BY name ASC
    sqlite3_stmt* stmt = nullptr;
    std::vector<ModelData> models;

    if (sqlite3_prepare_v2(impl->db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare getAllModels (ModelData) statement: " + std::string(sqlite3_errmsg(impl->db)));
    }
    auto stmt_guard = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>{stmt, sqlite3_finalize};

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ModelData model;
        model.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const unsigned char* name_text = sqlite3_column_text(stmt, 1);
        model.name = name_text ? reinterpret_cast<const char*>(name_text) : model.id; // Fallback to ID if name is NULL
        // If ModelData is expanded, retrieve other parameters here.
        models.push_back(model);
    }

    if (sqlite3_errcode(impl->db) != SQLITE_OK && sqlite3_errcode(impl->db) != SQLITE_DONE) {
         throw std::runtime_error("getAllModels (ModelData) failed during step: " + std::string(sqlite3_errmsg(impl->db)));
    }
    return models;
} // <<< --- ADDED CLOSING BRACE FOR getAllModels()

void PersistenceManager::replaceModelsInDB(const std::vector<ModelData>& models) {
    beginTransaction();
    try {
        clearModelsTable();
        for (const auto& model : models) {
            insertOrUpdateModel(model);
        }
        commitTransaction();
    } catch (const std::exception& e) {
        rollbackTransaction();
        // Re-throw the exception to be handled by the caller
        throw std::runtime_error("Failed to replace models in DB: " + std::string(e.what()));
    }
}

// Method to get model name by ID (for GUI display, placeholder)
std::optional<std::string> PersistenceManager::getModelNameById(const std::string& model_id) {
    // Placeholder implementation:
    // In a real scenario, this would query the 'models' table:
    // const char* sql = "SELECT name FROM models WHERE id = ?";
    // sqlite3_stmt* stmt;
    // if (sqlite3_prepare_v2(impl->db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
    //     sqlite3_bind_text(stmt, 1, model_id.c_str(), -1, SQLITE_STATIC);
    //     if (sqlite3_step(stmt) == SQLITE_ROW) {
    //         const unsigned char* name = sqlite3_column_text(stmt, 0);
    //         if (name) {
    //             sqlite3_finalize(stmt);
    //             return reinterpret_cast<const char*>(name);
    //         }
    //     }
    //     sqlite3_finalize(stmt);
    // }
    // For now, as the 'models' table might not be fully populated or its exact schema for 'name'
    // might vary, we'll return the model_id itself if it's not empty, or nullopt.
    // This allows the GUI to at least display the ID if the name isn't found.
    if (!model_id.empty()) {
        // Simulate a DB lookup for a "friendly name" if available, otherwise return ID
        // This part would be replaced by actual DB query to 'models' table
        if (model_id == "phi3:mini") return "Phi-3 Mini";
        if (model_id == "gpt-4-turbo") return "GPT-4 Turbo";
        if (model_id == "claude-3-opus") return "Claude 3 Opus";
        return model_id; // Fallback to returning the ID itself
    }
    return std::nullopt;
}

// Settings Management
// (The following PersistenceManager::Impl methods for settings are defined earlier in the file)
// void PersistenceManager::Impl::saveSetting(const std::string& key, const std::string& value) { ... }
// std::optional<std::string> PersistenceManager::Impl::loadSetting(const std::string& key) { ... }

// PersistenceManager public methods for settings
// Stray code removed.
// The functions getAllModels() and getModelNameById() are now correctly defined above.
// The Settings Management section correctly follows.

// Selected model ID management - REMOVED, use save/loadSetting directly
// void PersistenceManager::saveSelectedModelId(const std::string& model_id) {
//     impl->saveSetting("selected_model_id", model_id);
// }
//
// std::optional<std::string> PersistenceManager::loadSelectedModelId() {
//     return impl->loadSetting("selected_model_id");
// }

// Settings Management Implementation (delegating to Impl)
void PersistenceManager::saveSetting(const std::string& key, const std::string& value) {
    impl->saveSetting(key, value);
}

std::optional<std::string> PersistenceManager::loadSetting(const std::string& key) {
    return impl->loadSetting(key);
}
