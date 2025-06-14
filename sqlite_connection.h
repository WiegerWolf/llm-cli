#pragma once

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include "model_types.h" // For ModelData

struct sqlite3; // Forward declaration for SQLite database handle
struct sqlite3_stmt;

class SQLiteConnection {
public:
    SQLiteConnection();
    ~SQLiteConnection();

    SQLiteConnection(const SQLiteConnection&) = delete;
    SQLiteConnection& operator=(const SQLiteConnection&) = delete;

    void exec(const std::string& sql);
    void exec(const char* sql);

    sqlite3* getDbHandle();

private:
    sqlite3* db = nullptr;
};