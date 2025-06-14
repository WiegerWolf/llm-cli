#pragma once

#include "database_fwd.h"
#include <optional>
#include <string>

namespace app {
namespace db {

/*
 * Manages persistence for application settings.
 * This class handles all key-value operations on the `settings` table.
 */
class SettingsStore {
public:
    /// Constructs a SettingsStore using an existing database connection.
    explicit SettingsStore(SQLiteConnection& db_conn);

    /// Saves a key-value setting to the database.
    void saveSetting(const std::string& key, const std::string& value);

    /// Loads a setting's value by its key.
    std::optional<std::string> loadSetting(const std::string& key);

private:
    SQLiteConnection& m_db_conn;
};

} // namespace db
} // namespace app
