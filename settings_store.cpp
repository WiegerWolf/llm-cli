#include "settings_store.h"
#include "sqlite_connection.h"
#include <sqlite3.h>
#include <memory>
#include <stdexcept>

namespace app {
namespace db {
namespace {
struct SQLiteStmtDeleter {
    void operator()(sqlite3_stmt* stmt) const {
        if (stmt) {
            sqlite3_finalize(stmt);
        }
    }
};
using unique_sqlite_stmt_ptr = std::unique_ptr<sqlite3_stmt, SQLiteStmtDeleter>;
} // end anonymous namespace

SettingsStore::SettingsStore(SQLiteConnection& db_conn) : m_db_conn(db_conn) {}

void SettingsStore::saveSetting(const std::string& key, const std::string& value) {
    const char* sql = "INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db_conn.getDbHandle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare saveSetting statement: " + std::string(sqlite3_errmsg(m_db_conn.getDbHandle())));
    }
    unique_sqlite_stmt_ptr stmt_guard(stmt);
    if (sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC) != SQLITE_OK) {
        throw std::runtime_error("Failed to bind key in saveSetting: " + std::string(sqlite3_errmsg(m_db_conn.getDbHandle())));
    }
    if (sqlite3_bind_text(stmt, 2, value.c_str(), -1, SQLITE_STATIC) != SQLITE_OK) {
        throw std::runtime_error("Failed to bind value in saveSetting: " + std::string(sqlite3_errmsg(m_db_conn.getDbHandle())));
    }
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        throw std::runtime_error("saveSetting failed: " + std::string(sqlite3_errmsg(m_db_conn.getDbHandle())));
    }
}
std::optional<std::string> SettingsStore::loadSetting(const std::string& key) {
    const char* sql = "SELECT value FROM settings WHERE key = ?";
    sqlite3_stmt* stmt = nullptr;
    std::optional<std::string> result = std::nullopt;
    if (sqlite3_prepare_v2(m_db_conn.getDbHandle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (stmt) sqlite3_finalize(stmt);
        return std::nullopt;
    }
    unique_sqlite_stmt_ptr stmt_guard(stmt);
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
} // namespace db
} // namespace app