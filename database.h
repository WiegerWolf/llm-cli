#pragma once

#include "database_fwd.h"
#include "message_store.h"
#include "model_store.h"
#include "settings_store.h"
#include <memory>
#include <vector>
#include <string>

// The Database class serves as a facade, providing a unified, high-level
// interface for all persistence operations. It owns the underlying SQLite
// connection and the individual data stores.
class Database {
public:
    Database();
    ~Database();

    // Message-related operations
    void saveUserMessage(const std::string& content);
    void saveAssistantMessage(const std::string& content, const std::string& model_id);
    void saveToolMessage(const std::string& content);
    void cleanupOrphanedToolMessages();
    std::vector<app::db::Message> getContextHistory(size_t max_pairs = 10);
    std::vector<app::db::Message> getHistoryRange(const std::string& start_time, const std::string& end_time, size_t limit = 50);

    // Model-related operations
    void clearModelsTable();
    void insertOrUpdateModel(const ModelData& model);
    std::vector<ModelData> getAllModels();
    std::optional<ModelData> getModelById(const std::string& model_id);
    std::optional<std::string> getModelNameById(const std::string& model_id);
    void replaceModelsInDB(const std::vector<ModelData>& models);

    // Settings-related operations
    void saveSetting(const std::string& key, const std::string& value);
    std::optional<std::string> loadSetting(const std::string& key);

    // Transaction management
    void beginTransaction();
    void commitTransaction();
    void rollbackTransaction();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
