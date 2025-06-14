#pragma once

#include "database_fwd.h"
#include <string>
#include <optional>

class SettingsStore {
public:
    explicit SettingsStore(SQLiteConnection& db_conn);

    void saveSetting(const std::string& key, const std::string& value);
    std::optional<std::string> loadSetting(const std::string& key);

private:
    SQLiteConnection& m_db_conn;
};