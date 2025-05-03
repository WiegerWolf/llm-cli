#pragma once

#include "ui_interface.h" // Include the base class definition
#include <optional>
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <utility> // Added for std::pair
#include <condition_variable>

// Forward declaration for GLFW window handle
struct GLFWwindow;

class GuiInterface : public UserInterface {
public:
    GuiInterface();
    virtual ~GuiInterface() override;

    // Implementation of the UserInterface contract
    virtual std::optional<std::string> promptUserInput() override;
    virtual void displayOutput(const std::string& output) override;
    virtual void displayError(const std::string& error) override;
    virtual void displayStatus(const std::string& status) override;
    virtual void initialize() override;
    virtual void shutdown() override;

    // Public method to get the window handle (needed by main_gui.cpp)
    GLFWwindow* getWindow() const;

    // Methods for thread-safe communication (placeholders for Stage 4)
    void queueOutput(const std::string& output);
    void queueError(const std::string& error);
    void queueStatus(const std::string& status);
    std::vector<std::string> getQueuedOutputs();
    std::vector<std::string> getQueuedErrors();
    std::vector<std::string> getQueuedStatuses();
    void submitInput(const std::string& input);


    // --- Getters for GUI State (Stage 3) ---
    const std::vector<std::string>& getOutputHistory() const;
    const std::string& getStatusText() const;
    char* getInputBuffer(); // Returns pointer to internal buffer
    size_t getInputBufferSize() const; // Returns size of internal buffer

private:
    GLFWwindow* window = nullptr;

    // --- GUI State Members (Stage 3) ---
    static constexpr size_t INPUT_BUFFER_SIZE = 1024;
    char input_buf[INPUT_BUFFER_SIZE]; // Fixed-size buffer for ImGui::InputText
    std::vector<std::string> output_history;
    std::string status_text = "Ready";

    // --- Threading members (for Stage 4) ---
    // Mutexes for thread safety
    std::mutex display_mutex; // To protect output_history and status_text
    std::mutex input_mutex;   // To protect input_queue and input_ready flag

    // Queues for inter-thread communication
    std::queue<std::string> output_queue; // Note: These queues (output, error, status) might be replaced or used differently with display_queue in Stage 4
    std::queue<std::string> error_queue;
    std::queue<std::string> status_queue;
    std::queue<std::string> input_queue; // For user input submitted via GUI
    std::queue<std::pair<std::string, int>> display_queue; // For queuing display updates (text, type)
    std::condition_variable input_cv; // To signal when input is available
    bool input_ready = false;
    bool shutdown_requested = false; // Flag to signal shutdown to worker thread
};
