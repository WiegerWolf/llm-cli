#pragma once

#include "ui_interface.h" // Include the base class definition
#include "database.h"     // For PersistenceManager
#include <optional>
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <utility> // Added for std::pair
#include <condition_variable>
#include <thread>  // Added for Stage 4
#include <atomic>  // Added for Stage 4
#include <imgui.h> // Required for ImVec2, ImFont
#include <map>     // Required for ModelEntry if it uses map, or for getAllModels return type transformation
#include <core/id_types.h> // NodeIdType definition
#include <chrono>      // std::chrono::time_point
 
// Forward declaration for ImFont
struct ImFont;
 
// --- Message History Structure (Issue #8 Refactor) ---
// Moved from main_gui.cpp
enum class MessageType {
    USER_INPUT, // Note: User input is added directly in main_gui.cpp, not via this queue
    LLM_RESPONSE,
    STATUS,
    ERROR,
    USER_REPLY // Added for graph-based replies
};

struct HistoryMessage {
    NodeIdType message_id; // Unique identifier for the message (64-bit)
    MessageType type;
    std::string content;
    std::optional<std::string> model_id; // Changed for backward compatibility
    std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> timestamp; // High-resolution timestamp
    NodeIdType parent_id; // Parent message identifier or kInvalidNodeId if none
};
// --- End Message History Structure ---

// --- Theme Type Enum (Issue #18) ---
enum class ThemeType {
    DARK,
    WHITE
};
// --- End Theme Type Enum ---

// Forward declaration for GLFW window handle
struct GLFWwindow;

class GuiInterface : public UserInterface {
public:
    // --- Model Entry Structure (Part III GUI Changes) ---
    struct ModelEntry {
        std::string id;
        std::string name;
        // Potentially other metadata if needed for display in the future
    };
    // --- End Model Entry Structure ---
    GuiInterface(Database& db_manager); // Modified constructor
    virtual ~GuiInterface() override;

// Prevent copying/moving
    GuiInterface(const GuiInterface&)            = delete;
    GuiInterface& operator=(const GuiInterface&) = delete;
    GuiInterface(GuiInterface&&)                 = delete;
    GuiInterface& operator=(GuiInterface&&)      = delete;
    // Implementation of the UserInterface contract
    virtual std::optional<std::string> promptUserInput() override;
    virtual void displayOutput(const std::string& output, const std::string& model_id) override;
    virtual void displayError(const std::string& error) override;
    virtual void displayStatus(const std::string& status) override;
    virtual void initialize() override;
    virtual void shutdown() override;
    virtual bool isGuiMode() const override;

    // --- Implementation for Model Loading UI Feedback (Part V) ---
    virtual void setLoadingModelsState(bool isLoading) override;
    virtual void updateModelsList(const std::vector<ModelData>& models) override;
    // --- End Implementation for Model Loading UI Feedback ---

    // Public method to get the window handle (needed by main_gui.cpp)
    GLFWwindow* getWindow() const;
    float getCurrentFontSize() const { return current_font_size; }

    // --- Thread-safe methods for communication (Stage 4) ---
    void requestShutdown(); // Called by GUI thread to signal shutdown
    void sendInputToWorker(const std::string& input); // Called by GUI thread to send input
    ThemeType getTheme() const { return current_theme; } // Getter for current theme

    // --- Getters for GUI State (Stage 3) ---
    char* getInputBuffer(); // Returns pointer to internal buffer
    size_t getInputBufferSize() const; // Returns size of internal buffer

    // --- Thread-safe methods for communication (Stage 4) ---
    std::vector<HistoryMessage> processDisplayQueue(); // Returns drained messages

    // Method for GUI thread to get and clear accumulated scroll offsets
    ImVec2 getAndClearScrollOffsets();

    // --- Model Selection Methods (Part III GUI Changes / Updated Part V) ---
    std::vector<ModelEntry> getAvailableModelsForUI() const; // Renamed for clarity
    void setSelectedModelInUI(const std::string& model_id); // Renamed for clarity
    std::string getSelectedModelIdFromUI() const; // Renamed for clarity
    bool areModelsLoadingInUI() const; // Added for Part V
    // --- End Model Selection Methods ---
 
    // --- Font Accessors (Added for Model Dropdown Icons) ---
    ImFont* GetMainFont() const { return main_font; }
    ImFont* GetSmallFont() const { return small_font; }
    // --- End Font Accessors ---

    Database& getDbManager();
 
public:
    // Public members for direct access from callbacks and utils
    GLFWwindow* window = nullptr;
    float accumulated_scroll_x = 0.0f;
    float accumulated_scroll_y = 0.0f;
    std::mutex input_mutex;
    ImFont* main_font = nullptr;
    ImFont* small_font = nullptr;
    float current_font_size = 18.0f;
    bool font_rebuild_requested = false;
    float requested_font_size = 18.0f;
    ThemeType current_theme = ThemeType::DARK;

    // --- GUI State Members (Stage 3) ---
    static constexpr size_t INPUT_BUFFER_SIZE = 1024;
    char input_buf[INPUT_BUFFER_SIZE]; // Fixed-size buffer for ImGui::InputText

    // --- Threading members (for Stage 4) ---

    // Mutexes for thread safety
    std::mutex display_mutex; // To protect display_queue

    // Queues for inter-thread communication
    std::queue<std::string> input_queue; // For user input submitted via GUI
    std::queue<HistoryMessage> display_queue; // Updated for Issue #8
    std::condition_variable input_cv; // To signal when input is available
    std::atomic<bool> shutdown_requested{false}; // Updated for Stage 4
private:
    // Helper for DRY message enqueueing
    void enqueueDisplayMessage(MessageType type,
                               const std::string& content,
                               const std::optional<std::string>& model_id = std::nullopt);

    Database& db_manager_ref; // Reference to Database

    // --- Model Selection State (Part III GUI Changes / Updated Part V) ---
    std::string current_selected_model_id_in_ui; // Renamed for clarity
    // const std::string default_model_id = "phi3:mini"; // This is now DEFAULT_MODEL_ID from config.h
    std::vector<ModelEntry> available_models_for_ui; // Cache for UI display
    std::atomic<bool> models_are_loading_in_ui{false}; // For UI feedback
    mutable std::mutex models_ui_mutex; // To protect available_models_for_ui and current_selected_model_id_in_ui
    // --- End Model Selection State ---

    // --- End Model Selection State ---
};
