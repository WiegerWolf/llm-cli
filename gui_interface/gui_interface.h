#pragma once

#include "ui_interface.h" // Include the base class definition
#include <optional>
#include <string>
#include <vector>
#include <queue>
#include <mutex>
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


private:
    GLFWwindow* window = nullptr;

    // --- Threading members (for Stage 4) ---
    std::mutex mtx; // Mutex for protecting shared data
    std::queue<std::string> output_queue;
    std::queue<std::string> error_queue;
    std::queue<std::string> status_queue;
    std::queue<std::string> input_queue;
    std::condition_variable input_cv; // To signal when input is available
    bool input_ready = false;
    bool shutdown_requested = false; // Flag to signal shutdown to worker thread
};
