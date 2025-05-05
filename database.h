#pragma once
#include <vector>
#include <string>
#include <memory>
#include <optional> // Added for std::optional

struct Message {
    std::string role;
    std::string content;
    int id = 0;
    std::string timestamp; // Added timestamp field
};

class PersistenceManager {
public:
    PersistenceManager();
    ~PersistenceManager();
    
    void saveUserMessage(const std::string& content);
    void saveAssistantMessage(const std::string& content);
    void saveToolMessage(const std::string& content); // Added for tool results
    void cleanupOrphanedToolMessages(); // Added to clean up orphaned tool messages
    std::vector<Message> getContextHistory(size_t max_pairs = 10); // Gets recent context for API call
    // Changed signature to use time range and limit
    std::vector<Message> getHistoryRange(const std::string& start_time, const std::string& end_time, size_t limit = 50);

    // Transaction management
    void beginTransaction();
    void commitTransaction();
    void rollbackTransaction();
// Settings management
    void saveSetting(const std::string& key, const std::string& value);
    std::optional<std::string> loadSetting(const std::string& key);

private:
    // Forward declaration for the Pimpl (Pointer to Implementation) idiom
    // This hides the private implementation details (like the SQLite handle)
    // from the header file, reducing compile-time dependencies.
    struct Impl;
    std::unique_ptr<Impl> impl; // Owning pointer to the implementation object
};
