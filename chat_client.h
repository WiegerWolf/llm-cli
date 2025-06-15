#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <future>

#include "config.h"   // For OPENROUTER_API_URL_MODELS and DEFAULT_MODEL_ID
#include "database.h" // Includes Message struct
#include "tools.h"    // Includes ToolManager

// Forward-declaration for UI abstraction
class UserInterface;

// Now define ChatClient fully
class ChatClient {
private:
    Database& db; // Changed to reference
    ToolManager toolManager;
    UserInterface& ui; // Add reference to the UI
    std::string api_base = "https://openrouter.ai/api/v1/chat/completions";

    // --- Model Selection State (Part III GUI Changes) ---
    std::string active_model_id;
    // --- End Model Selection State ---

    // For model initialization and management
    std::atomic<bool> models_loading{false}; // For UI feedback
    std::future<void> model_load_future;     // For asynchronous loading

public:
    // Constructor now requires a UserInterface reference
    explicit ChatClient(UserInterface& ui_ref, Database& db_ref); // Added db_ref
    ~ChatClient(); // Destructor

    // Public method for making API calls (used by web_research tool)
    std::string makeApiCall(const std::vector<app::db::Message>& context, bool use_tools = false);

    // --- Model Selection Method (Part III GUI Changes) ---
    void setActiveModel(const std::string& model_id);
    // --- End Model Selection Method ---

    // Main application loop
    void run(); // Removed std::stop_token for now

public:
    void initialize_model_manager(); // Called at startup
    bool are_models_loading() const { return models_loading.load(); } // For UI to check status

    // Accessors for chat::turn functions
    Database& getDB() { return db; }
    ToolManager& getToolManager() { return toolManager; }
    UserInterface& getUI() { return ui; }
    const std::string& getActiveModelId() const { return active_model_id; }
    const std::string& getApiBase() const { return api_base; }
};
