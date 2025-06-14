#pragma once

#include "database_fwd.h"
#include <string>
#include <vector>
#include <optional>
#include "model_types.h"

struct Message {
    int id = 0;
    std::string role;
    std::string content;
    std::string timestamp;
    std::optional<std::string> model_id;
};

class MessageStore {
public:
    explicit MessageStore(SQLiteConnection& db_conn);

    void saveUserMessage(const std::string& content);
    void saveAssistantMessage(const std::string& content, const std::string& model_id);
    void saveToolMessage(const std::string& content);
    void cleanupOrphanedToolMessages();
    std::vector<Message> getContextHistory(size_t max_pairs = 10);
    std::vector<Message> getHistoryRange(const std::string& start_time, const std::string& end_time, size_t limit = 50);

private:
    SQLiteConnection& m_db_conn;
    void insertMessage(const Message& msg);
};