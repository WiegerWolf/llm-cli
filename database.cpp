#include <iostream>
#include <string>
#include <vector>
#include <sqlite3.h>
#include <cstdlib>
#include <stdexcept>
#include <filesystem>

struct Message {
    std::string role;
    std::string content;
};

// Initialize database connection and create tables if needed
sqlite3* initDatabase() {
    sqlite3* db = nullptr;
    std::string dbPath = std::string(getenv("HOME")) + "/.llm-cli-chat.db";
    
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        std::string error = sqlite3_errmsg(db);
        sqlite3_close(db);
        throw std::runtime_error("Cannot open database: " + error);
    }
    
    // Create table if it doesn't exist
    const char* createTableSQL = R"(
        CREATE TABLE IF NOT EXISTS messages (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            role TEXT NOT NULL,
            content TEXT NOT NULL
        );
    )";
    
    char* errMsg = nullptr;
    if (sqlite3_exec(db, createTableSQL, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string error = errMsg;
        sqlite3_free(errMsg);
        sqlite3_close(db);
        throw std::runtime_error("SQL error: " + error);
    }
    
    return db;
}

// Save messages to database
void saveHistoryToDatabase(const std::vector<Message>& history) {
    sqlite3* db = initDatabase();
    
    // Begin transaction
    sqlite3_exec(db, "BEGIN TRANSACTION", nullptr, nullptr, nullptr);
    
    // Clear existing messages
    sqlite3_exec(db, "DELETE FROM messages", nullptr, nullptr, nullptr);
    
    // Prepare insert statement
    sqlite3_stmt* stmt;
    const char* insertSQL = "INSERT INTO messages (role, content) VALUES (?, ?)";
    sqlite3_prepare_v2(db, insertSQL, -1, &stmt, nullptr);
    
    // Insert each message
    for (const auto& msg : history) {
        sqlite3_bind_text(stmt, 1, msg.role.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, msg.content.c_str(), -1, SQLITE_STATIC);
        
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::string error = sqlite3_errmsg(db);
            sqlite3_finalize(stmt);
            sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
            sqlite3_close(db);
            throw std::runtime_error("Failed to insert message: " + error);
        }
        
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
    }
    
    // Commit transaction and clean up
    sqlite3_finalize(stmt);
    sqlite3_exec(db, "COMMIT", nullptr, nullptr, nullptr);
    sqlite3_close(db);
}

// Load messages from database
std::vector<Message> loadHistoryFromDatabase() {
    std::vector<Message> history;
    sqlite3* db = nullptr;
    
    try {
        db = initDatabase();
        
        // Prepare select statement
        sqlite3_stmt* stmt;
        const char* selectSQL = "SELECT role, content FROM messages ORDER BY id ASC";
        
        if (sqlite3_prepare_v2(db, selectSQL, -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error(std::string("Failed to prepare statement: ") + sqlite3_errmsg(db));
        }
        
        // Fetch rows
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* role = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            const char* content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            
            history.push_back({
                role ? std::string(role) : "",
                content ? std::string(content) : ""
            });
        }
        
        sqlite3_finalize(stmt);
    } catch (const std::exception& e) {
        std::cerr << "Database error: " << e.what() << std::endl;
        // Return empty history on error
    }
    
    if (db) {
        sqlite3_close(db);
    }
    
    // Add default system message if history is empty
    if (history.empty()) {
        history.push_back({"system", "You are a helpful assistant."});
    }
    
    return history;
}
