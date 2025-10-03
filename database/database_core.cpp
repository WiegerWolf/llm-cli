#include "database_core.h"
#include <stdexcept>
#include <iostream>
#include <cstdlib>
#include <cstring>

namespace database {

namespace {
// Helper function to get home directory path (cross-platform)
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
} // anonymous namespace

DatabaseCore::DatabaseCore() : db_(nullptr) {
    // Construct the database path using the cross-platform helper
    std::filesystem::path db_dir_path = get_home_directory_path();
    std::string final_db_path_str;

    if (!db_dir_path.empty()) {
        try {
            // Ensure the .llm-cli directory exists in the home directory
            std::filesystem::path app_config_dir = db_dir_path / ".llm-cli";
            if (!std::filesystem::exists(app_config_dir)) {
                std::filesystem::create_directories(app_config_dir);
            }
            // Define the database file path within this directory
            std::filesystem::path db_file_path = app_config_dir / "llm_chat_history.db";
            final_db_path_str = db_file_path.string();
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "Filesystem error constructing database path in home directory: " << e.what()
                      << ". Using current directory as fallback." << std::endl;
            final_db_path_str = "llm_chat_history.db"; // Fallback to current directory
        }
    } else {
        std::cerr << "Warning: Could not determine home directory. Using current directory for database." << std::endl;
        final_db_path_str = "llm_chat_history.db"; // Fallback to current directory
    }
    
    std::string path = final_db_path_str;

    // Open the SQLite database connection
    if(sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
        std::string err_msg = "Database connection failed: ";
        if (db_) { // sqlite3_open sets db even on error
            err_msg += sqlite3_errmsg(db_);
            sqlite3_close(db_); // Close the handle if opened partially
            db_ = nullptr;
        } else {
            err_msg += "Could not allocate memory for database handle.";
        }
        throw std::runtime_error(err_msg);
    }

    // Initialize schema and run migrations
    // Wrap in try-catch to ensure db_ is closed if initialization fails
    try {
        initializeSchema();
        runMigrations();
        
        // Enable Write-Ahead Logging (WAL) mode for better concurrency
        exec("PRAGMA journal_mode=WAL");
    } catch (...) {
        // Initialization failed after successful open - must close handle to prevent leak
        // The destructor won't run if constructor throws, so we clean up manually
        sqlite3_close(db_);
        db_ = nullptr;
        throw;  // Re-throw the original exception
    }
}

DatabaseCore::~DatabaseCore() {
    if (db_) {
        sqlite3_close(db_);
    }
}

void DatabaseCore::beginTransaction() {
    exec("BEGIN");
}

void DatabaseCore::commitTransaction() {
    exec("COMMIT");
}

void DatabaseCore::rollbackTransaction() {
    exec("ROLLBACK");
}

unique_stmt_ptr DatabaseCore::prepareStatement(const std::string& sql) {
    sqlite3_stmt* raw_stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &raw_stmt, nullptr) != SQLITE_OK) {
        std::string err_msg = "Failed to prepare statement: ";
        err_msg += sqlite3_errmsg(db_);
        if (raw_stmt) {
            sqlite3_finalize(raw_stmt);
        }
        throw std::runtime_error(err_msg);
    }
    return unique_stmt_ptr(raw_stmt);
}

void DatabaseCore::exec(const char* sql) {
    char* err_msg_ptr = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &err_msg_ptr) != SQLITE_OK) {
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

void DatabaseCore::exec(const std::string& sql) {
    char* err_msg_ptr = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err_msg_ptr) != SQLITE_OK) {
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

std::filesystem::path DatabaseCore::getDatabasePath() {
    std::filesystem::path db_dir_path = get_home_directory_path();
    
    if (!db_dir_path.empty()) {
        std::filesystem::path app_config_dir = db_dir_path / ".llm-cli";
        return app_config_dir / "llm_chat_history.db";
    }
    
    return "llm_chat_history.db";
}

void DatabaseCore::initializeSchema() {
    // Define the database schema
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
    
    exec(schema);
}

void DatabaseCore::runMigrations() {
    // Migration: Add model_id to messages table if it doesn't exist
    bool model_id_column_exists = false;
    sqlite3_stmt* raw_stmt_check_column = nullptr;
    std::string pragma_sql = "PRAGMA table_info('messages');";

    if (sqlite3_prepare_v2(db_, pragma_sql.c_str(), -1, &raw_stmt_check_column, nullptr) == SQLITE_OK) {
        unique_stmt_ptr stmt_check_column_guard(raw_stmt_check_column);

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
        err_msg += sqlite3_errmsg(db_);
        if (raw_stmt_check_column) {
             sqlite3_finalize(raw_stmt_check_column);
        }
        throw std::runtime_error(err_msg);
    }

    if (!model_id_column_exists) {
        exec("ALTER TABLE messages ADD COLUMN model_id TEXT;");
    }
}

} // namespace database