#include "gui_interface.h"
#include <stdexcept>
#include <iostream> // For error reporting during init/shutdown

// Include GUI library headers
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

// Helper function for GLFW errors
static void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

// Flag to track if ImGui backends were successfully initialized
static bool imgui_init_done = false;

GuiInterface::GuiInterface() {
    // Initialize the input buffer
    input_buf[0] = '\0';
}

GuiInterface::~GuiInterface() {
    // Ensure shutdown is called, although it should be called explicitly
    if (window) {
        shutdown();
    }
}

void GuiInterface::initialize() {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char* glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.3 + GLSL 330
    const char* glsl_version = "#version 330";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // Core profile
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Generally not needed unless targeting very old drivers
#endif

    // Create window with graphics context
    window = glfwCreateWindow(1280, 720, "LLM Client GUI", nullptr, nullptr);
    if (window == nullptr) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // --- Initialize ImGui ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    // Optional ImGui flags (commented out as not currently used):
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    // if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    //     ImGuiStyle& style = ImGui::GetStyle();
    //     style.WindowRounding = 0.0f;
    //     style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    // }


    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsClassic(); // Alternative style


    // Setup Platform/Renderer backends
    if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) {
         ImGui::DestroyContext();
         glfwDestroyWindow(window);
         glfwTerminate();
         throw std::runtime_error("Failed to initialize ImGui GLFW backend");
    }
    if (!ImGui_ImplOpenGL3_Init(glsl_version)) {
         ImGui_ImplGlfw_Shutdown();
         ImGui::DestroyContext();
         glfwDestroyWindow(window);
         glfwTerminate();
         throw std::runtime_error("Failed to initialize ImGui OpenGL3 backend");
    }


    // Load Fonts (optional - commented out as default font is used)
    // ImGuiIO& io = ImGui::GetIO();
    // io.Fonts->AddFontDefault();
    // io.Fonts->AddFontFromFileTTF("path/to/font.ttf", 16.0f);
    // io.Fonts->Build(); // Build font atlas if using custom fonts

    imgui_init_done = true; // Mark ImGui as fully initialized
    std::cout << "GUI Initialized Successfully." << std::endl;
}

void GuiInterface::shutdown() {
    if (!window) return; // Prevent double shutdown

    std::cout << "Shutting down GUI..." << std::endl;

    // Cleanup ImGui only if it was successfully initialized
    if (imgui_init_done) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    // Cleanup GLFW
    glfwDestroyWindow(window);
    glfwTerminate();
    window = nullptr; // Mark as shut down
    std::cout << "GUI Shutdown Complete." << std::endl;
}

GLFWwindow* GuiInterface::getWindow() const {
    return window;
}

// --- Getters for GUI State (Used by main_gui.cpp) ---

char* GuiInterface::getInputBuffer() {
    // Returns a pointer to the internal input buffer.
    // The caller (main_gui) must respect the buffer size.
    // Allows main_gui to directly modify the buffer (e.g., clearing after send).
    return input_buf;
}

size_t GuiInterface::getInputBufferSize() const {
    // Returns the compile-time constant size of the input buffer.
    return INPUT_BUFFER_SIZE;
}

// --- Implementation of UserInterface contract (Thread-Safe Communication) ---

// This method is called by the *worker thread* (ChatClient) when it needs user input.
// It blocks the worker thread until input is provided by the GUI thread or shutdown is requested.
std::optional<std::string> GuiInterface::promptUserInput() {
    // Acquire a unique lock on the input mutex. The lock is automatically released when exiting the scope.
    std::unique_lock<std::mutex> lock(input_mutex);

    // Wait on the condition variable. The thread goes to sleep until:
    // 1. `input_cv.notify_one()` is called (by `sendInputToWorker` or `requestShutdown`).
    // 2. The predicate `[this]{ return input_ready.load() || shutdown_requested.load(); }` returns true.
    // The predicate protects against spurious wakeups and ensures we only proceed when input is actually ready or shutdown is signaled.
    input_cv.wait(lock, [this]{ return input_ready.load() || shutdown_requested.load(); });

    // Check if shutdown was requested while waiting.
    if (shutdown_requested.load()) {
        return std::nullopt; // Signal the worker to terminate.
    }

    // If woken up and not shutting down, input should be ready.
    // Double-check the queue isn't empty (although the predicate should guarantee it).
    if (input_ready.load() && !input_queue.empty()) {
        std::string input = std::move(input_queue.front()); // Use move for efficiency.
        input_queue.pop();
        input_ready = false; // Reset the flag: the input has been consumed.
        return input;        // Return the user input to the worker thread.
    }

    // Fallback/Error case: Should not be reached if logic is correct.
    // If a spurious wakeup occurred and the queue is empty despite input_ready being true (unlikely),
    // or if input_ready became false between the predicate check and here.
    input_ready = false; // Ensure flag is reset.
    return std::nullopt; // Indicate no input available this time.
}

// Called by the *worker thread* to add regular output to the display queue.
void GuiInterface::displayOutput(const std::string& output) {
    // Lock the display mutex to ensure exclusive access to the display queue.
    std::lock_guard<std::mutex> lock(display_mutex);
    // Push the message and its type onto the queue for the GUI thread to process.
    display_queue.push({output, DisplayMessageType::OUTPUT});
    // The GUI thread periodically calls processDisplayQueue to check this queue.
}

// Called by the *worker thread* to add error messages to the display queue.
void GuiInterface::displayError(const std::string& error) {
    // Lock the display mutex.
    std::lock_guard<std::mutex> lock(display_mutex);
    // Push the error message and its type onto the queue.
    display_queue.push({error, DisplayMessageType::ERROR});
}

// Called by the *worker thread* to update the status text in the display queue.
void GuiInterface::displayStatus(const std::string& status) {
    // Lock the display mutex.
    std::lock_guard<std::mutex> lock(display_mutex);
    // Push the status message and its type onto the queue.
    display_queue.push({status, DisplayMessageType::STATUS});
}


// --- Methods for GUI thread to interact with Worker thread ---

// Called by the *GUI thread* (e.g., when the window close button is pressed).
void GuiInterface::requestShutdown() {
    // Atomically set the shutdown flag. This is checked by the worker thread.
    shutdown_requested = true;
    // Notify the condition variable. This wakes up the worker thread if it's
    // currently blocked waiting in `promptUserInput`.
    input_cv.notify_one();
}

// Called by the *GUI thread* when the user presses "Send" or Enter in the input box.
void GuiInterface::sendInputToWorker(const std::string& input) {
    // Use a separate scope for the lock_guard to ensure the mutex is released
    // *before* notifying the condition variable. This is important for performance
    // and avoids potential deadlocks if the woken thread immediately tries to lock the same mutex.
    {
        std::lock_guard<std::mutex> lock(input_mutex);
        // Add the user's input string to the queue.
        input_queue.push(input);
        // Atomically set the flag indicating input is ready.
        input_ready = true;
    } // input_mutex is released here.

    // Notify the condition variable. This wakes up the worker thread if it's
    // currently blocked waiting in `promptUserInput`.
    input_cv.notify_one();
}

// --- Method for GUI thread to process display updates ---

// Called by the *GUI thread* in its main render loop.
// Processes all messages currently in the display queue.
// Returns true if the history vector was modified (used for auto-scrolling).
bool GuiInterface::processDisplayQueue(std::vector<std::string>& history, std::string& status) {
    bool history_updated = false;
    // Lock the display mutex to safely access the queue from the GUI thread.
    std::lock_guard<std::mutex> lock(display_mutex);

    // Process all messages currently in the queue.
    while (!display_queue.empty()) {
        // Move the front element to a local variable before popping.
        auto msgPair = std::move(display_queue.front());
        display_queue.pop(); // Now it's safe to pop.

        // Use structured binding on the moved pair.
        const auto& [message, type] = msgPair;

        // Handle the message based on its type.
        switch (type) {
            case DisplayMessageType::OUTPUT:
                history.push_back(message); // Add to the local history vector in main_gui.cpp
                history_updated = true;
                break;
            case DisplayMessageType::ERROR:
                history.push_back("ERROR: " + message); // Prepend "ERROR:" for clarity
                history_updated = true;
                break;
            case DisplayMessageType::STATUS:
                status = message; // Update the local status string in main_gui.cpp
                break;
        }
        // Message is processed, loop continues or exits.
    }
    // Return whether the history content changed, so the GUI can auto-scroll.
    return history_updated;
}
