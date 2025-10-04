#pragma once

#include "database_core.h"
#include <string>
#include <vector>
#include <optional>

namespace database {

/**
 * Session struct represents a chat session
 */
struct Session {
    int id = 0;
    std::string title;
    std::string created_at;
    std::string last_message_at;
    int message_count = 0;
};

/**
 * SessionRepository - Encapsulates all session-related database operations
 *
 * Responsibilities:
 * - Session creation
 * - Session retrieval and listing
 * - Session title updates
 * - Session deletion
 */
class SessionRepository {
public:
    /**
     * Constructor
     * @param core Reference to DatabaseCore for connection access
     */
    explicit SessionRepository(DatabaseCore& core);

    /**
     * Create a new session
     * @param title Optional title for the session (default: "New Chat")
     * @return The ID of the newly created session
     */
    int createSession(const std::string& title = "New Chat");

    /**
     * Get all sessions ordered by most recent activity
     * @return Vector of all sessions
     */
    std::vector<Session> getAllSessions();

    /**
     * Get a specific session by ID
     * @param session_id The session ID
     * @return Optional containing the session if found
     */
    std::optional<Session> getSessionById(int session_id);

    /**
     * Update session title
     * @param session_id The session ID
     * @param title The new title
     */
    void updateSessionTitle(int session_id, const std::string& title);

    /**
     * Delete a session and all its messages
     * @param session_id The session ID
     */
    void deleteSession(int session_id);

    /**
     * Get the most recent session ID, or create one if none exists
     * @return The session ID
     */
    int getOrCreateDefaultSession();

private:
    DatabaseCore& core_;
};

} // namespace database
