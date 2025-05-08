#pragma once
#include <vector>
#include <string>
#include <memory>
#include <optional> // Added for std::optional
#include "model_types.h" // For ModelData struct
struct sqlite3; // Forward declaration for SQLite database handle

struct Message {
    std::string role;
    std::string content;
    int id = 0;
    std::string timestamp; // Added timestamp field
    std::string model_id; // <-- New field (changed from optional)
};

// Model struct is now ModelData, defined in model_types.h

class PersistenceManager {
public:
    PersistenceManager();
    ~PersistenceManager();
    
    void saveUserMessage(const std::string& content);
    void saveAssistantMessage(const std::string& content, const std::string& model_id);
    void saveToolMessage(const std::string& content); // Added for tool results
    void cleanupOrphanedToolMessages(); // Added to clean up orphaned tool messages
    std::vector<Message> getContextHistory(size_t max_pairs = 10); // Gets recent context for API call
    // Changed signature to use time range and limit
    std::vector<Message> getHistoryRange(const std::string& start_time, const std::string& end_time, size_t limit = 50);

    // Model specific operations
    // void saveOrUpdateModel(const Model& model); // Replaced by insertOrUpdateModel
    // std::optional<Model> getModelById(const std::string& id); // To be updated or removed if not used by new logic
    // std::vector<Model> getAllModels(bool orderByName = true); // Replaced by getAllModels()
    // void clearAllModels(); // Replaced by clearModelsTable()

    void clearModelsTable();
    void insertOrUpdateModel(const ModelData& model);
    std::vector<ModelData> getAllModels(); // New signature
    std::optional<std::string> getModelNameById(const std::string& model_id); // For GUI display

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
    struct Impl {
        Impl(); // Constructor
        ~Impl(); // Destructor

        sqlite3* db = nullptr; // Pointer to the SQLite database connection

        // Settings management
        void saveSetting(const std::string& key, const std::string& value);
        std::optional<std::string> loadSetting(const std::string& key);

        // General SQL execution
        void exec(const std::string& sql);
        void exec(const char* sql); // Overload for const char*

        // Message insertion
        void insertMessage(const Message& msg);
    };
    std::unique_ptr<Impl> impl; // Owning pointer to the implementation object
};
