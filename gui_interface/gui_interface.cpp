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

GuiInterface::GuiInterface() = default;

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
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking (optional)
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;   // Enable Multi-Viewport / Platform Windows (optional)
    //io.ConfigViewportsNoAutoMerge = true;
    //io.ConfigViewportsNoTaskBarIcon = true;


    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

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

// --- Stub implementations ---

std::optional<std::string> GuiInterface::promptUserInput() {
    // This will be implemented properly in Stage 4 using thread synchronization
    std::unique_lock<std::mutex> lock(mtx);
    input_cv.wait(lock, [this]{ return input_ready || shutdown_requested; });

    if (shutdown_requested) {
        return std::nullopt; // Signal exit if shutdown was requested
    }

    if (!input_queue.empty()) {
        std::string input = input_queue.front();
        input_queue.pop();
        input_ready = !input_queue.empty(); // Reset flag if queue is now empty
        return input;
    }
    // Should not happen if logic is correct, but return nullopt as fallback
    input_ready = false;
    return std::nullopt;
}

void GuiInterface::displayOutput(const std::string& output) {
    // Queue for GUI thread (Stage 4)
    queueOutput(output);
}

void GuiInterface::displayError(const std::string& error) {
    // Queue for GUI thread (Stage 4)
    queueError(error);
}

void GuiInterface::displayStatus(const std::string& status) {
    // Queue for GUI thread (Stage 4)
    queueStatus(status);
}

// --- Thread-safe queue methods (Implementations for Stage 4) ---

void GuiInterface::queueOutput(const std::string& output) {
    std::lock_guard<std::mutex> lock(mtx);
    output_queue.push(output);
}

void GuiInterface::queueError(const std::string& error) {
    std::lock_guard<std::mutex> lock(mtx);
    error_queue.push(error);
}

void GuiInterface::queueStatus(const std::string& status) {
    std::lock_guard<std::mutex> lock(mtx);
    status_queue.push(status);
}

std::vector<std::string> GuiInterface::getQueuedOutputs() {
    std::lock_guard<std::mutex> lock(mtx);
    std::vector<std::string> outputs;
    while (!output_queue.empty()) {
        outputs.push_back(output_queue.front());
        output_queue.pop();
    }
    return outputs;
}

std::vector<std::string> GuiInterface::getQueuedErrors() {
    std::lock_guard<std::mutex> lock(mtx);
    std::vector<std::string> errors;
    while (!error_queue.empty()) {
        errors.push_back(error_queue.front());
        error_queue.pop();
    }
    return errors;
}

std::vector<std::string> GuiInterface::getQueuedStatuses() {
    std::lock_guard<std::mutex> lock(mtx);
    std::vector<std::string> statuses;
    while (!status_queue.empty()) {
        statuses.push_back(status_queue.front());
        status_queue.pop();
    }
    return statuses;
}

void GuiInterface::submitInput(const std::string& input) {
    {
        std::lock_guard<std::mutex> lock(mtx);
        input_queue.push(input);
        input_ready = true;
    }
    input_cv.notify_one(); // Notify the waiting worker thread
}
