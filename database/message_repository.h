#pragma once

#include "database_core.h"
#include "../database.h"  // For Message struct
#include <vector>
#include <string>

namespace database {

/**
 * MessageRepository - Encapsulates all message-related database operations
 * 
 * Responsibilities:
 * - Message insertion (user, assistant, tool)
 * - Message retrieval with various filters
 * - Context history building for API calls
 * - Time-range queries for history viewing
 * - Orphaned tool message cleanup
 * - Tool message validation
 */
class MessageRepository {
public:
    /**
     * Constructor
     * @param core Reference to DatabaseCore for connection access
     */
    explicit MessageRepository(DatabaseCore& core);
    
    // Message insertion methods
    
    /**
     * Insert a user message into the database
     * @param content The message content
     */
    void insertUserMessage(const std::string& content);
    
    /**
     * Insert an assistant message into the database
     * @param content The message content
     * @param model_id The ID of the model that generated the response
     */
    void insertAssistantMessage(const std::string& content, const std::string& model_id);
    
    /**
     * Insert a tool message into the database
     * @param content The tool response content (must be valid JSON)
     * @throws std::runtime_error if content is not valid tool message JSON
     */
    void insertToolMessage(const std::string& content);
    
    // Message retrieval methods
    
    /**
     * Get recent conversation context for API calls
     * @param max_pairs Maximum number of user-assistant message pairs to retrieve
     * @return Vector of messages including system message and recent history
     */
    std::vector<Message> getContextHistory(size_t max_pairs = 10);
    
    /**
     * Get messages within a specific time range
     * @param start_time Start timestamp in SQLite datetime format
     * @param end_time End timestamp in SQLite datetime format
     * @param limit Maximum number of messages to retrieve
     * @return Vector of messages within the specified time range
     */
    std::vector<Message> getHistoryRange(const std::string& start_time,
                                          const std::string& end_time,
                                          size_t limit = 50);
    
    // Maintenance operations
    
    /**
     * Clean up orphaned tool messages (tool messages without preceding assistant message)
     * @throws std::runtime_error if cleanup fails
     */
    void cleanupOrphanedToolMessages();

private:
    DatabaseCore& core_;  // Reference to database core for connection access
    
    /**
     * Internal helper to insert a message
     * @param msg The message to insert
     */
    void insertMessage(const Message& msg);
    
    /**
     * Validate tool message content (must be valid JSON with required fields)
     * @param content The tool message content to validate
     * @throws std::runtime_error if validation fails
     */
    void validateToolMessage(const std::string& content);
    
    /**
     * Build a Message object from a database row
     * @param stmt The prepared statement pointing to a row
     * @return Message object populated from the row
     */
    Message buildMessageFromRow(sqlite3_stmt* stmt);
};

} // namespace database