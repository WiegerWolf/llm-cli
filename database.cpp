#include "database.h"
#include "sqlite_connection.h"

struct Database::Impl {
    app::db::SQLiteConnection connection;
    app::db::MessageStore message_store;
    app::db::ModelStore model_store;
    app::db::SettingsStore settings_store;

    Impl() :
        connection(),
        message_store(connection),
        model_store(connection),
        settings_store(connection)
    {}
};

Database::Database() : m_impl(std::make_unique<Impl>()) {}
Database::~Database() = default;

// Message Operations
void Database::saveUserMessage(const std::string& content) {
    m_impl->message_store.saveUserMessage(content);
}

void Database::saveAssistantMessage(const std::string& content, const std::string& model_id) {
    m_impl->message_store.saveAssistantMessage(content, model_id);
}

void Database::saveToolMessage(const std::string& content) {
    m_impl->message_store.saveToolMessage(content);
}

void Database::cleanupOrphanedToolMessages() {
    m_impl->message_store.cleanupOrphanedToolMessages();
}

std::vector<app::db::Message> Database::getContextHistory(size_t max_pairs) {
    return m_impl->message_store.getContextHistory(max_pairs);
}

std::vector<app::db::Message> Database::getHistoryRange(const std::string& start_time, const std::string& end_time, size_t limit) {
    return m_impl->message_store.getHistoryRange(start_time, end_time, limit);
}

// Model Operations
void Database::clearModelsTable() {
    m_impl->model_store.clearModelsTable();
}

void Database::insertOrUpdateModel(const ModelData& model) {
    m_impl->model_store.insertOrUpdateModel(model);
}

std::vector<ModelData> Database::getAllModels() {
    return m_impl->model_store.getAllModels();
}

std::optional<ModelData> Database::getModelById(const std::string& model_id) {
    return m_impl->model_store.getModelById(model_id);
}

std::optional<std::string> Database::getModelNameById(const std::string& model_id) {
    return m_impl->model_store.getModelNameById(model_id);
}

void Database::replaceModelsInDB(const std::vector<ModelData>& models) {
    m_impl->model_store.replaceModelsInDB(models);
}

// Settings Operations
void Database::saveSetting(const std::string& key, const std::string& value) {
    m_impl->settings_store.saveSetting(key, value);
}

std::optional<std::string> Database::loadSetting(const std::string& key) {
    return m_impl->settings_store.loadSetting(key);
}

// Transaction Management
void Database::beginTransaction() {
    m_impl->connection.exec("BEGIN");
}

void Database::commitTransaction() {
    m_impl->connection.exec("COMMIT");
}

void Database::rollbackTransaction() {
    m_impl->connection.exec("ROLLBACK");
}
