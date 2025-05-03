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
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Optional
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
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking (optional)
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;   // Enable Multi-Viewport / Platform Windows (optional)
    //io.ConfigViewportsNoAutoMerge = true;
    //io.ConfigViewportsNoTaskBarIcon = true;


    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    // if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    // {
    //     style.WindowRounding = 0.0f;
    //     style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    // }

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


    // Load Fonts (optional)
    // io.Fonts->AddFontDefault();
    // io.Fonts->AddFontFromFileTTF("path/to/font.ttf", 16.0f);
    // ImGui::GetIO().Fonts->Build(); // Build font atlas if using custom fonts

    std::cout << "GUI Initialized Successfully." << std::endl;
}

void GuiInterface::shutdown() {
    if (!window) return; // Prevent double shutdown

    std::cout << "Shutting down GUI..." << std::endl;

    // Cleanup ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // Cleanup GLFW
    glfwDestroyWindow(window);
    glfwTerminate();
    window = nullptr; // Mark as shut down
    std::cout << "GUI Shutdown Complete." << std::endl;
}

GLFWwindow* GuiInterface::getWindow() const {
    return window;
}

// --- Getters for GUI State (Stage 3) ---

const std::vector<std::string>& GuiInterface::getOutputHistory() const {
    // Note: Accessing this from the main thread while the worker might modify it
    // is NOT thread-safe. This will be addressed in Stage 4 with mutexes or queues.
    return output_history;
}

const std::string& GuiInterface::getStatusText() const {
    // Note: Accessing this from the main thread while the worker might modify it
    // is NOT thread-safe. This will be addressed in Stage 4 with mutexes or queues.
    return status_text;
}

char* GuiInterface::getInputBuffer() {
    // Returns a pointer to the internal buffer. The caller must respect the buffer size.
    // Note: Returning a non-const pointer allows modification (e.g., clearing).
    return input_buf;
}

size_t GuiInterface::getInputBufferSize() const {
    // Returns the compile-time constant size of the buffer.
    return INPUT_BUFFER_SIZE;
}
// --- Stub implementations ---

// --- Implementation of UserInterface contract (Thread-Safe for Stage 4) ---

std::optional<std::string> GuiInterface::promptUserInput() {
    // Called by the worker thread to wait for user input from the GUI thread
    std::unique_lock<std::mutex> lock(input_mutex);
    input_cv.wait(lock, [this]{ return input_ready.load() || shutdown_requested.load(); });

    if (shutdown_requested.load()) {
        return std::nullopt; // Shutdown requested while waiting
    }

    // Input must be ready if shutdown wasn't requested
    if (input_ready.load() && !input_queue.empty()) {
        std::string input = std::move(input_queue.front()); // Use move for efficiency
        input_queue.pop();
        input_ready = false; // Reset the flag after consuming the input
        return input;
    }

    // This state should ideally not be reached if logic is correct
    // (wait condition implies input_ready or shutdown_requested)
    // If it happens (e.g., spurious wakeup and queue empty), reset flag and return nullopt
    input_ready = false;
    return std::nullopt;
}

void GuiInterface::displayOutput(const std::string& output) {
    // Called by the worker thread to queue output for the GUI thread
    std::lock_guard<std::mutex> lock(display_mutex);
    display_queue.push({output, DisplayMessageType::OUTPUT});
    // Note: GUI thread needs to be signaled or periodically check this queue
}

void GuiInterface::displayError(const std::string& error) {
    // Called by the worker thread to queue an error for the GUI thread
    std::lock_guard<std::mutex> lock(display_mutex);
    display_queue.push({error, DisplayMessageType::ERROR});
    // Note: GUI thread needs to be signaled or periodically check this queue
}

void GuiInterface::displayStatus(const std::string& status) {
    // Called by the worker thread to queue a status update for the GUI thread
    std::lock_guard<std::mutex> lock(display_mutex);
    display_queue.push({status, DisplayMessageType::STATUS});
    // Note: GUI thread needs to be signaled or periodically check this queue
}


// --- Methods for GUI thread to interact with Worker thread (Stage 4) ---

void GuiInterface::requestShutdown() {
    // Called by the GUI thread (e.g., when window is closed)
    shutdown_requested = true;
    input_cv.notify_one(); // Wake up the worker thread if it's waiting in promptUserInput
}

void GuiInterface::sendInputToWorker(const std::string& input) {
    // Called by the GUI thread when the user submits input
    {
        std::lock_guard<std::mutex> lock(input_mutex);
        input_queue.push(input);
        input_ready = true;
    } // Mutex is released before notifying
    input_cv.notify_one(); // Notify the worker thread waiting in promptUserInput
}

// --- Method for GUI thread to process display updates (Stage 4) ---

bool GuiInterface::processDisplayQueue(std::vector<std::string>& history, std::string& status) {
    // Called by the GUI thread in its main loop
    bool history_updated = false;
    std::lock_guard<std::mutex> lock(display_mutex); // Lock the queue for processing

    while (!display_queue.empty()) {
        auto& [message, type] = display_queue.front(); // Get reference to the front pair

        switch (type) {
            case DisplayMessageType::OUTPUT:
                history.push_back(message);
                history_updated = true;
                break;
            case DisplayMessageType::ERROR:
                // Optionally prepend "ERROR: " or handle differently
                history.push_back("ERROR: " + message);
                history_updated = true;
                break;
            case DisplayMessageType::STATUS:
                status = message; // Update the status text directly
                break;
        }
        display_queue.pop(); // Remove the processed message
    }
    return history_updated; // Return true if history was modified
}
