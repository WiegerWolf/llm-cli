#pragma once
#include <vector>
#include <string>
#include <memory>
#include <optional>
#include "model_types.h"
struct sqlite3;

// Forward declaration for Session struct
namespace database {
    struct Session;
}

// Message struct represents a single message in the chat history.
struct Message {
    std::string role;
    std::string content;
    int id = 0;
    std::string timestamp;
    std::optional<std::string> model_id;
};

class PersistenceManager {
public:
    PersistenceManager();
    ~PersistenceManager();
    
    void saveUserMessage(const std::string& content);
    void saveAssistantMessage(const std::string& content, const std::string& model_id);
    void saveToolMessage(const std::string& content);
    void cleanupOrphanedToolMessages();
    std::vector<Message> getContextHistory(size_t max_pairs = 10);
    std::vector<Message> getHistoryRange(const std::string& start_time, const std::string& end_time, size_t limit = 50);

    // Model specific operations
    void clearModelsTable();
    void insertOrUpdateModel(const ModelData& model);
    std::vector<ModelData> getAllModels();
    std::optional<ModelData> getModelById(const std::string& model_id);
    std::optional<std::string> getModelNameById(const std::string& model_id);

    // Transaction management
    void beginTransaction();
    void commitTransaction();
    void rollbackTransaction();

    // Atomic replacement of models
    void replaceModelsInDB(const std::vector<ModelData>& models);

    // Settings management
    void saveSetting(const std::string& key, const std::string& value);
    std::optional<std::string> loadSetting(const std::string& key);

    // Session management
    int createSession(const std::string& title = "New Chat");
    std::vector<database::Session> getAllSessions();
    std::optional<database::Session> getSessionById(int session_id);
    void updateSessionTitle(int session_id, const std::string& title);
    void deleteSession(int session_id);
    int getOrCreateDefaultSession();
    void setCurrentSession(int session_id);
    int getCurrentSession() const;

private:
    // Forward declaration for the Pimpl idiom
    // Implementation is completely hidden in database.cpp
    struct Impl;
    std::unique_ptr<Impl> impl;
};
