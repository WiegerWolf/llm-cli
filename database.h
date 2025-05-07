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
    std::optional<std::string> model_id;
};

// Represents an AI model's data
struct Model {
    std::string id;
    std::string name;
    std::string description;
    int context_length;
    std::string pricing_prompt;
    std::string pricing_completion;
    std::string architecture_input_modalities; // JSON string
    std::string architecture_output_modalities; // JSON string
    std::string architecture_tokenizer;
    int top_provider_is_moderated; // 0 or 1 (boolean)
    std::string per_request_limits; // JSON string
    std::string supported_parameters; // JSON string
    long long created_at_api; // UNIX Timestamp (INTEGER)
    std::string last_updated_db; // Timestamp string (YYYY-MM-DD HH:MM:SS), retrieved from DB
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

    // Model specific operations
    void saveOrUpdateModel(const Model& model);
    std::optional<Model> getModelById(const std::string& id);
    std::vector<Model> getAllModels(bool orderByName = true);
    void clearAllModels();

    // Transaction management
    void beginTransaction();
    void commitTransaction();
    void rollbackTransaction();
// Settings management
    void saveSetting(const std::string& key, const std::string& value);
    std::optional<std::string> loadSetting(const std::string& key);

    // Selected model ID management
    void saveSelectedModelId(const std::string& model_id);
    std::optional<std::string> loadSelectedModelId();

private:
    // Forward declaration for the Pimpl (Pointer to Implementation) idiom
    // This hides the private implementation details (like the SQLite handle)
    // from the header file, reducing compile-time dependencies.
    struct Impl;
    std::unique_ptr<Impl> impl; // Owning pointer to the implementation object
};
