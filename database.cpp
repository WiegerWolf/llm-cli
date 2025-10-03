#include "database.h"
#include "database/database_core.h"
#include "database/message_repository.h"
#include "database/model_repository.h"
#include <memory>
#include <stdexcept>
#include <optional>

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

// Pimpl implementation using the new repository pattern
struct PersistenceManager::Impl {
    database::DatabaseCore core;
    database::MessageRepository messages;
    database::ModelRepository models;
    
    Impl() 
        : core()
        , messages(core)
        , models(core)
    {}
    
    // Settings management remains in Impl (simple operations)
    void saveSetting(const std::string& key, const std::string& value);
    std::optional<std::string> loadSetting(const std::string& key);
};

// Settings management implementation
void PersistenceManager::Impl::saveSetting(const std::string& key, const std::string& value) {
    const char* sql = "INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)";
    
    auto stmt = this->core.prepareStatement(sql);

    if (sqlite3_bind_text(stmt.get(), 1, key.c_str(), -1, SQLITE_STATIC) != SQLITE_OK) {
        throw std::runtime_error("Failed to bind key in Impl::saveSetting: " + std::string(sqlite3_errmsg(this->core.getConnection())));
    }
    if (sqlite3_bind_text(stmt.get(), 2, value.c_str(), -1, SQLITE_STATIC) != SQLITE_OK) {
        throw std::runtime_error("Failed to bind value in Impl::saveSetting: " + std::string(sqlite3_errmsg(this->core.getConnection())));
    }

    if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
        throw std::runtime_error("Impl::saveSetting failed: " + std::string(sqlite3_errmsg(this->core.getConnection())));
    }
}

std::optional<std::string> PersistenceManager::Impl::loadSetting(const std::string& key) {
    const char* sql = "SELECT value FROM settings WHERE key = ?";
    std::optional<std::string> result = std::nullopt;

    auto stmt = this->core.prepareStatement(sql);

    if (sqlite3_bind_text(stmt.get(), 1, key.c_str(), -1, SQLITE_STATIC) != SQLITE_OK) {
        return std::nullopt;
    }

    int step_result = sqlite3_step(stmt.get());
    if (step_result == SQLITE_ROW) {
        const unsigned char* text = sqlite3_column_text(stmt.get(), 0);
        if (text) {
            result = reinterpret_cast<const char*>(text);
        }
    } else if (step_result != SQLITE_DONE) {
        return std::nullopt;
    }
    return result;
}

// PersistenceManager public API implementation - delegates to repositories

PersistenceManager::PersistenceManager() : impl(std::make_unique<Impl>()) {}
PersistenceManager::~PersistenceManager() = default;

// Transaction Management - delegates to DatabaseCore
void PersistenceManager::beginTransaction() {
    impl->core.beginTransaction();
}

void PersistenceManager::commitTransaction() {
    impl->core.commitTransaction();
}

void PersistenceManager::rollbackTransaction() {
    impl->core.rollbackTransaction();
}

// Message operations - delegate to MessageRepository
void PersistenceManager::saveUserMessage(const std::string& content) {
    impl->messages.insertUserMessage(content);
}

void PersistenceManager::saveAssistantMessage(const std::string& content, const std::string& model_id) {
    impl->messages.insertAssistantMessage(content, model_id);
}

void PersistenceManager::saveToolMessage(const std::string& content) {
    impl->messages.insertToolMessage(content);
}

void PersistenceManager::cleanupOrphanedToolMessages() {
    impl->messages.cleanupOrphanedToolMessages();
}

std::vector<Message> PersistenceManager::getContextHistory(size_t max_pairs) {
    return impl->messages.getContextHistory(max_pairs);
}

std::vector<Message> PersistenceManager::getHistoryRange(const std::string& start_time, const std::string& end_time, size_t limit) {
    return impl->messages.getHistoryRange(start_time, end_time, limit);
}

// Model operations - delegate to ModelRepository
void PersistenceManager::clearModelsTable() {
    impl->models.clearAllModels();
}

void PersistenceManager::insertOrUpdateModel(const ModelData& model) {
    impl->models.insertOrUpdateModel(model);
}

std::vector<ModelData> PersistenceManager::getAllModels() {
    return impl->models.getAllModels();
}

std::optional<ModelData> PersistenceManager::getModelById(const std::string& model_id) {
    return impl->models.getModelById(model_id);
}

std::optional<std::string> PersistenceManager::getModelNameById(const std::string& model_id) {
    return impl->models.getModelNameById(model_id);
}

void PersistenceManager::replaceModelsInDB(const std::vector<ModelData>& models) {
    impl->models.replaceModels(models);
}

// Settings management - delegate to Impl
void PersistenceManager::saveSetting(const std::string& key, const std::string& value) {
    impl->saveSetting(key, value);
}

std::optional<std::string> PersistenceManager::loadSetting(const std::string& key) {
    return impl->loadSetting(key);
}
