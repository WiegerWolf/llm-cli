#pragma once

#include <sqlite3.h>
#include <memory>
#include <string>
#include <filesystem>

namespace database {

// RAII wrapper for sqlite3_stmt - ensures proper finalization
struct SQLiteStmtDeleter {
    void operator()(sqlite3_stmt* stmt) const {
        if (stmt) {
            sqlite3_finalize(stmt);
        }
    }
};

// Type alias for unique pointer to sqlite3_stmt with custom deleter
using unique_stmt_ptr = std::unique_ptr<sqlite3_stmt, SQLiteStmtDeleter>;

/**
 * DatabaseCore - Foundation layer for SQLite database operations
 * 
 * Responsibilities:
 * - SQLite connection lifecycle management (open/close)
 * - Database path resolution (cross-platform)
 * - Schema initialization and migrations
 * - Transaction management
 * - SQL execution utilities
 * - RAII wrappers for safe resource management
 */
class DatabaseCore {
public:
    /**
     * Constructor - Initializes database connection and schema
     * @throws std::runtime_error if connection fails or schema initialization fails
     */
    DatabaseCore();
    
    /**
     * Destructor - Ensures proper cleanup of database connection
     */
    ~DatabaseCore();
    
    // Delete copy operations (database connection should not be copied)
    DatabaseCore(const DatabaseCore&) = delete;
    DatabaseCore& operator=(const DatabaseCore&) = delete;
    
    // Transaction management
    void beginTransaction();
    void commitTransaction();
    void rollbackTransaction();
    
    /**
     * Prepare a SQL statement for execution
     * @param sql The SQL query to prepare
     * @return unique_stmt_ptr managing the prepared statement
     * @throws std::runtime_error if preparation fails
     */
    unique_stmt_ptr prepareStatement(const std::string& sql);
    
    /**
     * Execute a simple SQL statement without expecting results
     * @param sql The SQL statement to execute
     * @throws std::runtime_error if execution fails
     */
    void exec(const std::string& sql);
    
    /**
     * Execute a simple SQL statement without expecting results
     * @param sql The SQL statement to execute (C-string)
     * @throws std::runtime_error if execution fails
     */
    void exec(const char* sql);
    
    /**
     * Get direct access to the SQLite connection
     * @return Pointer to the sqlite3 connection
     * @note For use by repositories only - handle with care
     */
    sqlite3* getConnection() { return db_; }

private:
    sqlite3* db_;  // SQLite database connection handle
    
    /**
     * Determine the appropriate database file path (cross-platform)
     * @return Filesystem path to the database file
     */
    std::filesystem::path getDatabasePath();
    
    /**
     * Ensure parent directory exists for the given path
     * @param path The file path whose parent directory should exist
     * @return true if directory exists or was created, false on error
     */
    bool ensureDirectoryExists(const std::filesystem::path& path);
    
    /**
     * Initialize database schema (create tables if they don't exist)
     */
    void initializeSchema();
    
    /**
     * Run any necessary database migrations
     */
    void runMigrations();
};

} // namespace database