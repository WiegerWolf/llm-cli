#include "sqlite_connection.h"
#include <sqlite3.h>
#include <memory>
#include <stdexcept>
#include <filesystem>
#include <cstdlib>
#include <iostream>
#include <cstring>

namespace { // Anonymous namespace for helper
std::filesystem::path get_home_directory_path() {
    #ifdef _WIN32
        const char* userprofile = std::getenv("USERPROFILE");
        if (userprofile) {
            return std::filesystem::path(userprofile);
        }
        const char* homedrive = std::getenv("HOMEDRIVE");
        const char* homepath = std::getenv("HOMEPATH");
        if (homedrive && homepath) {
            return std::filesystem::path(homedrive) / homepath;
        }
    #else // POSIX-like systems
        const char* home_env = std::getenv("HOME");
        if (home_env) {
            return std::filesystem::path(home_env);
        }
    #endif
    return ""; // Return empty path if home directory cannot be determined
}
} // end anonymous namespace

namespace { // Or make these part of a utility struct/namespace if preferred
struct SQLiteStmtDeleter {
    void operator()(sqlite3_stmt* stmt) const {
        if (stmt) {
            sqlite3_finalize(stmt);
        }
    }
};
using unique_sqlite_stmt_ptr = std::unique_ptr<sqlite3_stmt, SQLiteStmtDeleter>;
} // end anonymous namespace


SQLiteConnection::SQLiteConnection() : db(nullptr) {
    std::filesystem::path db_dir_path = get_home_directory_path();
    std::string final_db_path_str;

    if (!db_dir_path.empty()) {
        try {
            std::filesystem::path app_config_dir = db_dir_path / ".llm-cli";
            if (!std::filesystem::exists(app_config_dir)) {
                std::filesystem::create_directories(app_config_dir);
            }
            std::filesystem::path db_file_path = app_config_dir / "llm_chat_history.db";
            final_db_path_str = db_file_path.string();
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "Filesystem error constructing database path in home directory: " << e.what()
                      << ". Using current directory as fallback." << std::endl;
            final_db_path_str = "llm_chat_history.db";
        }
    } else {
        std::cerr << "Warning: Could not determine home directory. Using current directory for database." << std::endl;
        final_db_path_str = "llm_chat_history.db";
    }
    
    std::string path = final_db_path_str;

    if(sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
        std::string err_msg = "Database connection failed: ";
        if (db) {
            err_msg += sqlite3_errmsg(db);
            sqlite3_close(db);
            db = nullptr;
        } else {
            err_msg += "Could not allocate memory for database handle.";
        }
        throw std::runtime_error(err_msg);
    }

    const char* schema = R"(
        CREATE TABLE IF NOT EXISTS messages (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            role TEXT CHECK(role IN ('system','user','assistant', 'tool')),
            content TEXT,
            model_id TEXT
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
    this->exec(schema);

    bool model_id_column_exists = false;
    sqlite3_stmt* raw_stmt_check_column = nullptr;
    std::string pragma_sql = "PRAGMA table_info('messages');";

    if (sqlite3_prepare_v2(db, pragma_sql.c_str(), -1, &raw_stmt_check_column, nullptr) == SQLITE_OK) {
        unique_sqlite_stmt_ptr stmt_check_column_guard(raw_stmt_check_column);

        while (sqlite3_step(stmt_check_column_guard.get()) == SQLITE_ROW) {
            const unsigned char* col_name_text = sqlite3_column_text(stmt_check_column_guard.get(), 1);
            if (col_name_text) {
                std::string col_name(reinterpret_cast<const char*>(col_name_text));
                if (col_name == "model_id") {
                    model_id_column_exists = true;
                    break;
                }
            }
        }
    } else {
        std::string err_msg = "Failed to prepare PRAGMA table_info('messages'): ";
        err_msg += sqlite3_errmsg(db);
        if (raw_stmt_check_column) {
             sqlite3_finalize(raw_stmt_check_column);
        }
        throw std::runtime_error(err_msg);
    }

    if (!model_id_column_exists) {
        this->exec("ALTER TABLE messages ADD COLUMN model_id TEXT;");
    }

    this->exec("PRAGMA journal_mode=WAL");
}

SQLiteConnection::~SQLiteConnection() {
    if (db) {
        sqlite3_close(db);
    }
}

void SQLiteConnection::exec(const char* sql) {
    char* err_msg_ptr = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &err_msg_ptr) != SQLITE_OK) {
        std::string error_message_str = "SQL error executing '";
        error_message_str += sql;
        error_message_str += "': ";
        if (err_msg_ptr) {
            error_message_str += err_msg_ptr;
            sqlite3_free(err_msg_ptr);
        } else {
            error_message_str += "Unknown SQLite error (no specific message provided by sqlite3_exec)";
        }
        throw std::runtime_error(error_message_str);
    }
}

void SQLiteConnection::exec(const std::string& sql) {
    char* err_msg_ptr = nullptr;
    if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err_msg_ptr) != SQLITE_OK) {
        std::string error_message_str = "SQL error executing '";
        error_message_str += sql;
        error_message_str += "': ";
        if (err_msg_ptr) {
            error_message_str += err_msg_ptr;
            sqlite3_free(err_msg_ptr);
        } else {
            error_message_str += "Unknown SQLite error (no specific message provided by sqlite3_exec)";
        }
        throw std::runtime_error(error_message_str);
    }
}

sqlite3* SQLiteConnection::getDbHandle() {
    return db;
}