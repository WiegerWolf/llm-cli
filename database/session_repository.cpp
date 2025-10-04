#include "session_repository.h"
#include <stdexcept>

namespace database {

SessionRepository::SessionRepository(DatabaseCore& core)
    : core_(core) {
}

int SessionRepository::createSession(const std::string& title) {
    const char* sql = "INSERT INTO sessions (title) VALUES (?)";

    auto stmt = core_.prepareStatement(sql);
    sqlite3_bind_text(stmt.get(), 1, title.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
        throw std::runtime_error("Failed to create session: " +
                                std::string(sqlite3_errmsg(core_.getConnection())));
    }

    return static_cast<int>(sqlite3_last_insert_rowid(core_.getConnection()));
}

std::vector<Session> SessionRepository::getAllSessions() {
    const char* sql = R"(
        SELECT
            s.id,
            s.title,
            s.created_at,
            COALESCE(MAX(m.timestamp), s.created_at) as last_message_at,
            COUNT(m.id) as message_count
        FROM sessions s
        LEFT JOIN messages m ON s.id = m.session_id
        GROUP BY s.id
        ORDER BY last_message_at DESC
    )";

    auto stmt = core_.prepareStatement(sql);

    std::vector<Session> sessions;
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        Session session;
        session.id = sqlite3_column_int(stmt.get(), 0);
        session.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));
        session.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 2));
        session.last_message_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 3));
        session.message_count = sqlite3_column_int(stmt.get(), 4);
        sessions.push_back(session);
    }

    return sessions;
}

std::optional<Session> SessionRepository::getSessionById(int session_id) {
    const char* sql = R"(
        SELECT
            s.id,
            s.title,
            s.created_at,
            COALESCE(MAX(m.timestamp), s.created_at) as last_message_at,
            COUNT(m.id) as message_count
        FROM sessions s
        LEFT JOIN messages m ON s.id = m.session_id
        WHERE s.id = ?
        GROUP BY s.id
    )";

    auto stmt = core_.prepareStatement(sql);
    sqlite3_bind_int(stmt.get(), 1, session_id);

    if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        Session session;
        session.id = sqlite3_column_int(stmt.get(), 0);
        session.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));
        session.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 2));
        session.last_message_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 3));
        session.message_count = sqlite3_column_int(stmt.get(), 4);
        return session;
    }

    return std::nullopt;
}

void SessionRepository::updateSessionTitle(int session_id, const std::string& title) {
    const char* sql = "UPDATE sessions SET title = ? WHERE id = ?";

    auto stmt = core_.prepareStatement(sql);
    sqlite3_bind_text(stmt.get(), 1, title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt.get(), 2, session_id);

    if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
        throw std::runtime_error("Failed to update session title: " +
                                std::string(sqlite3_errmsg(core_.getConnection())));
    }
}

void SessionRepository::deleteSession(int session_id) {
    // Delete all messages in the session first
    const char* delete_messages_sql = "DELETE FROM messages WHERE session_id = ?";
    auto delete_messages_stmt = core_.prepareStatement(delete_messages_sql);
    sqlite3_bind_int(delete_messages_stmt.get(), 1, session_id);

    if (sqlite3_step(delete_messages_stmt.get()) != SQLITE_DONE) {
        throw std::runtime_error("Failed to delete session messages: " +
                                std::string(sqlite3_errmsg(core_.getConnection())));
    }

    // Then delete the session
    const char* delete_session_sql = "DELETE FROM sessions WHERE id = ?";
    auto delete_session_stmt = core_.prepareStatement(delete_session_sql);
    sqlite3_bind_int(delete_session_stmt.get(), 1, session_id);

    if (sqlite3_step(delete_session_stmt.get()) != SQLITE_DONE) {
        throw std::runtime_error("Failed to delete session: " +
                                std::string(sqlite3_errmsg(core_.getConnection())));
    }
}

int SessionRepository::getOrCreateDefaultSession() {
    // Try to get the most recent session
    const char* get_sql = "SELECT id FROM sessions ORDER BY created_at DESC LIMIT 1";
    auto stmt = core_.prepareStatement(get_sql);

    if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        return sqlite3_column_int(stmt.get(), 0);
    }

    // No sessions exist, create a default one
    return createSession("Default Chat");
}

} // namespace database
