#include "database.h"
#include <sqlite3.h>
#include <memory>
#include <stdexcept>
#include <iostream>
#include <cstdlib>

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
                role TEXT CHECK(role IN ('system','user','assistant')),
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

void PersistenceManager::saveUserMessage(const std::string& content) {
    impl->exec("BEGIN");
    try {
        impl->insertMessage({"user", content});
        impl->exec("COMMIT");
    } catch(...) {
        impl->exec("ROLLBACK");
        throw;
    }
}

void PersistenceManager::saveAssistantMessage(const std::string& content) {
    impl->exec("BEGIN");
    try {
        impl->insertMessage({"assistant", content});
        impl->exec("COMMIT");
    } catch(...) {
        impl->exec("ROLLBACK");
        throw;
    }
}

std::vector<Message> PersistenceManager::getContextHistory(size_t max_pairs) {
    const std::string sql = R"(
        WITH system_msg AS (
            SELECT * FROM messages WHERE role='system' ORDER BY id DESC LIMIT 1
        ),
        recent_msgs AS (
            SELECT * FROM messages 
            WHERE role IN ('user','assistant')
            ORDER BY id DESC 
            LIMIT ?
        )
        SELECT id, role, content FROM system_msg
        UNION ALL
        SELECT id, role, content FROM recent_msgs ORDER BY id ASC
    )";

    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(impl->db, sql.c_str(), -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, max_pairs * 2); // 2 messages per pair
    
    std::vector<Message> history;
    while(sqlite3_step(stmt) == SQLITE_ROW) {
        history.push_back({
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)),
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)),
            sqlite3_column_int(stmt, 0)
        });
    }
    sqlite3_finalize(stmt);
    
    // Add default system message if history is empty
    if (history.empty()) {
        history.push_back({"system", "You are a helpful assistant."});
    }
    
    return history;
}

void PersistenceManager::trimDatabaseHistory(size_t keep_pairs) {
    const std::string sql = R"(
        DELETE FROM messages WHERE id IN (
            SELECT id FROM messages
            WHERE role IN ('user','assistant')
            ORDER BY id DESC
            OFFSET ?
        )
    )";
    
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(impl->db, sql.c_str(), -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, keep_pairs * 2);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}
