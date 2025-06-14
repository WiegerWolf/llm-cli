#pragma once

#include <memory>
#include <string>
#include <vector>
#include <optional>

struct sqlite3;      // Forward declaration for SQLite database handle
struct sqlite3_stmt; // Forward declaration for SQLite prepared statement

namespace app {
namespace db {

/*
 * Low-level RAII wrapper around a SQLite database connection.
 * All public methods forward to the underlying C API while enforcing
 * exception-based error handling and ownership semantics.
 */
class SQLiteConnection {
public:
    SQLiteConnection();
    ~SQLiteConnection();

    SQLiteConnection(const SQLiteConnection&) = delete;
    SQLiteConnection& operator=(const SQLiteConnection&) = delete;

    // Execute one or more SQL statements separated by semicolons.
    void exec(const std::string& sql);
    void exec(const char* sql);

    // Return the raw sqlite3* handle (use with care).
    sqlite3* getDbHandle();

private:
    sqlite3* db = nullptr;
};

} // namespace db
} // namespace app
