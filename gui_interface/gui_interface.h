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
};
