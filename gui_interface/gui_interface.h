#pragma once

#include "ui_interface.h" // Include the base class definition
#include <optional>
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <utility> // Added for std::pair
#include <condition_variable>
#include <thread>  // Added for Stage 4
#include <atomic>  // Added for Stage 4
#include <imgui.h> // Required for ImVec2

// --- Message History Structure (Issue #8 Refactor) ---
// Moved from main_gui.cpp
enum class MessageType {
    USER_INPUT, // Note: User input is added directly in main_gui.cpp, not via this queue
    LLM_RESPONSE,
    STATUS,
    ERROR
};

struct HistoryMessage {
    MessageType type;
    std::string content;
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
    GuiInterface();
    virtual ~GuiInterface() override;

// Prevent copying/moving
    GuiInterface(const GuiInterface&)            = delete;
    GuiInterface& operator=(const GuiInterface&) = delete;
    GuiInterface(GuiInterface&&)                 = delete;
    GuiInterface& operator=(GuiInterface&&)      = delete;
    // Implementation of the UserInterface contract
    virtual std::optional<std::string> promptUserInput() override;
    virtual void displayOutput(const std::string& output) override;
    virtual void displayError(const std::string& error) override;
    virtual void displayStatus(const std::string& status) override;
    virtual void initialize() override;
    virtual void shutdown() override;
    virtual bool isGuiMode() const override;

    // Public method to get the window handle (needed by main_gui.cpp)
    GLFWwindow* getWindow() const;

    // --- Theme Setting Method (Issue #18) ---
    void setTheme(ThemeType theme);
    // --- End Theme Setting Method ---

// --- Font Size Control (Issue #19) ---
    void changeFontSize(float delta);
void resetFontSize();
float getCurrentFontSize() const { return current_font_size; }
    // --- End Font Size Control ---
void setInitialFontSize(float size); // Added for persistence
    // --- Thread-safe methods for communication (Stage 4) ---
    void requestShutdown(); // Called by GUI thread to signal shutdown
    void sendInputToWorker(const std::string& input); // Called by GUI thread to send input

    // --- Getters for GUI State (Stage 3) ---
    char* getInputBuffer(); // Returns pointer to internal buffer
    size_t getInputBufferSize() const; // Returns size of internal buffer

    // --- Thread-safe methods for communication (Stage 4) ---
    std::vector<HistoryMessage> processDisplayQueue(); // Returns drained messages

    // Method for GUI thread to get and clear accumulated scroll offsets
    ImVec2 getAndClearScrollOffsets();

public: // Changed from private to allow access from static callback
    GLFWwindow* window = nullptr;
    float accumulated_scroll_x = 0.0f;
    float accumulated_scroll_y = 0.0f;

    // --- GUI State Members (Stage 3) ---
    static constexpr size_t INPUT_BUFFER_SIZE = 1024;
    char input_buf[INPUT_BUFFER_SIZE]; // Fixed-size buffer for ImGui::InputText

    // --- Threading members (for Stage 4) ---

    // Mutexes for thread safety
    std::mutex display_mutex; // To protect display_queue (updated purpose)
    std::mutex input_mutex;   // To protect input_queue and input_ready flag

    // Queues for inter-thread communication
    std::queue<std::string> input_queue; // For user input submitted via GUI
    std::queue<HistoryMessage> display_queue; // Updated for Issue #8
    std::condition_variable input_cv; // To signal when input is available
    std::atomic<bool> shutdown_requested{false}; // Updated for Stage 4
private:
    // --- Font Size State & Helpers (Issue #19) ---
    float current_font_size = 18.0f; // Default font size
    bool font_rebuild_requested = false; // Flag to defer rebuild
    float requested_font_size = 18.0f;   // Target size for deferred rebuild

    void loadFonts(float size);
    void rebuildFontAtlas(float new_size); // Keep private

public: // Add public method to process the request
    void processFontRebuildRequest(); // Called by main loop before NewFrame
    // --- End Font Size State & Helpers ---
};
