#pragma once

#include "database_fwd.h"
#include <optional>
#include <string>
#include <vector>

namespace app {
namespace db {

/// Represents a single message in the chat history.
struct Message {
    int id = 0;
    std::string role;
    std::string content;
    std::string timestamp;
    std::optional<std::string> model_id;
};

/*
 * High-level interface for chat message persistence.
 * This class handles all CRUD operations on the `messages` table.
 */
class MessageStore {
public:
    /// Constructs a MessageStore using an existing database connection.
    explicit MessageStore(SQLiteConnection& db_conn);

    /// Saves a user message to the history.
    void saveUserMessage(const std::string& content);

    /// Saves an assistant's message, optionally with the model ID.
    void saveAssistantMessage(const std::string& content, const std::string& model_id);

    /// Saves a tool invocation message.
    void saveToolMessage(const std::string& content);

    /// Removes tool messages that don't have a corresponding assistant message.
    void cleanupOrphanedToolMessages();

    /// Retrieves the most recent `max_pairs` of messages for context.
    std::vector<Message> getContextHistory(size_t max_pairs = 10);

    /// Retrieves all messages within a specific time range, up to a limit.
    std::vector<Message> getHistoryRange(const std::string& start_time,
                                       const std::string& end_time,
                                       size_t limit = 50);

private:
    SQLiteConnection& m_db_conn;
};

namespace detail {
    /// Internal helper functions for MessageStore.
    struct MessageStoreDetail {
        static void insertMessage(SQLiteConnection& db_conn, const Message& msg);
    };
} // namespace detail


} // namespace db
} // namespace app
