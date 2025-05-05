#include <cmath>
#include <cstdio> // For fprintf
#include "gui_interface.h"
#include <stdexcept>
#include <iostream> // For error reporting during init/shutdown
#include <algorithm> // For std::clamp (Issue #19)

// Include GUI library headers
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include "../resources/noto_sans_font.h" // Include the generated font header

// Forward declaration
class GuiInterface;

// Custom GLFW scroll callback
static void custom_glfw_scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    GuiInterface* gui_ui = static_cast<GuiInterface*>(glfwGetWindowUserPointer(window));
    if (gui_ui) {
        // Accumulate scroll offsets with mutex protection
        std::lock_guard<std::mutex> lock(gui_ui->input_mutex);
        gui_ui->accumulated_scroll_x += static_cast<float>(xoffset);
        gui_ui->accumulated_scroll_y += static_cast<float>(yoffset);
    }
    // Allow ImGui's backend to also process the scroll event if needed
    // Note: ImGui's backend also uses the scroll callback to update its internal state.
    // Chaining the callback ensures ImGui receives the event.
    // ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset); // Removed to avoid potential conflicts
}


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

    // Set the window user pointer to the GuiInterface instance
    glfwSetWindowUserPointer(window, this);

    // Set the custom scroll callback
    glfwSetScrollCallback(window, custom_glfw_scroll_callback);

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


    // Setup Dear ImGui style - REMOVED HARDCODED STYLE (Issue #18)
    // ImGui::StyleColorsDark(); // This is now handled by setTheme
    // ImGui::StyleColorsClassic(); // Alternative style


    // Setup Platform/Renderer backends
    if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) {
         ImGui::DestroyContext();
         glfwDestroyWindow(window);
         window = nullptr; // Prevent double free in destructor
         glfwTerminate();
         imgui_init_done = false; // Ensure flag is false on failure
         throw std::runtime_error("Failed to initialize ImGui GLFW backend");
    }
    if (!ImGui_ImplOpenGL3_Init(glsl_version)) {
         ImGui_ImplGlfw_Shutdown();
         ImGui::DestroyContext();
         glfwDestroyWindow(window);
         window = nullptr; // Prevent double free in destructor
         glfwTerminate();
         imgui_init_done = false; // Ensure flag is false on failure
         throw std::runtime_error("Failed to initialize ImGui OpenGL3 backend");
    }


    // Load Fonts using the helper function (Issue #19)
    this->loadFonts(this->current_font_size);

    // IMPORTANT: Build the font atlas AFTER adding all fonts/ranges
    io.Fonts->Build();

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
    input_cv.wait(lock, [this]{ return !input_queue.empty() || shutdown_requested.load(); });

    // Check if shutdown was requested while waiting.
    if (shutdown_requested.load()) {
        return std::nullopt; // Signal the worker to terminate.
    }

    // If woken up and not shutting down, check if there's input in the queue.
    // The wait predicate now directly checks the queue, so this check is the primary path.
    if (!input_queue.empty()) {
        std::string input = std::move(input_queue.front()); // Use move for efficiency.
        input_queue.pop();
        // No need to reset input_ready here, as the predicate relies on queue emptiness.
        return input;        // Return the user input to the worker thread.
    }

    // Fallback/Error case: Should not be reached if logic is correct.
    // If a spurious wakeup occurred and the queue is empty (unlikely),
    // or if the queue became empty between the predicate check and here.
    return std::nullopt; // Indicate no input available this time.
}

// Called by the *worker thread* to add regular output to the display queue.
void GuiInterface::displayOutput(const std::string& output) {
    // Lock the display mutex to ensure exclusive access to the display queue.
    std::lock_guard<std::mutex> lock(display_mutex);
    // Push the message and its type onto the queue for the GUI thread to process.
    display_queue.push({MessageType::LLM_RESPONSE, output}); // Updated for Issue #8
    // The GUI thread periodically calls processDisplayQueue to check this queue.
}

// Called by the *worker thread* to add error messages to the display queue.
void GuiInterface::displayError(const std::string& error) {
    // Lock the display mutex.
    std::lock_guard<std::mutex> lock(display_mutex);
    // Push the error message and its type onto the queue.
    display_queue.push({MessageType::ERROR, error}); // Updated for Issue #8
}

// Called by the *worker thread* to update the status text in the display queue.
void GuiInterface::displayStatus(const std::string& status) {
    // Lock the display mutex.
    std::lock_guard<std::mutex> lock(display_mutex);
    // Push the status message and its type onto the queue.
    display_queue.push({MessageType::STATUS, status}); // Updated for Issue #8
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
        // The condition variable predicate now checks the queue directly.
    } // input_mutex is released here.

    // Notify the condition variable. This wakes up the worker thread if it's
    // currently blocked waiting in `promptUserInput`.
    input_cv.notify_one();
}

// --- Method for GUI thread to process display updates ---

// Called by the *GUI thread* in its main render loop.
// Processes all messages currently in the display queue.
// Returns a vector containing all messages drained from the internal display queue.
std::vector<HistoryMessage> GuiInterface::processDisplayQueue() {
    std::vector<HistoryMessage> transferred_messages; // Local vector to hold drained messages
    transferred_messages.reserve(display_queue.size());
    // Lock the display mutex to safely access the queue from the GUI thread.
    std::lock_guard<std::mutex> lock(display_mutex);

    // Process all messages currently in the queue.
    while (!display_queue.empty()) {
        // Move the front element directly into the result vector before popping.
        transferred_messages.push_back(std::move(display_queue.front()));
        display_queue.pop(); // Now it's safe to pop.
    }
    // Return the vector containing all drained messages.
    return transferred_messages;
}

ImVec2 GuiInterface::getAndClearScrollOffsets() {
    // Lock the input mutex as accumulated_scroll_x/y are modified by the scroll callback
    std::lock_guard<std::mutex> lock(input_mutex);
    ImVec2 offsets(accumulated_scroll_x, accumulated_scroll_y);
    accumulated_scroll_x = 0.0f;
    accumulated_scroll_y = 0.0f;
    return offsets;
}

// Implementation for isGuiMode - GUI is always GUI mode.
bool GuiInterface::isGuiMode() const {
    return true;
}

// --- Theme Helper Functions (Issue #18) ---
void applyDarkTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // Custom Dark Theme Colors
    colors[ImGuiCol_Text]                   = ImVec4(0.88f, 0.88f, 0.88f, 1.00f); // Light Grey Text
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.12f, 0.12f, 0.12f, 1.00f); // Very Dark Grey
    colors[ImGuiCol_ChildBg]                = ImVec4(0.15f, 0.15f, 0.15f, 1.00f); // Slightly Lighter Dark Grey
    colors[ImGuiCol_PopupBg]                = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
    colors[ImGuiCol_Border]                 = ImVec4(0.31f, 0.31f, 0.31f, 0.50f); // Dark Grey Border
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.23f, 0.23f, 0.23f, 1.00f); // Darker Grey Frame
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.29f, 0.29f, 0.29f, 1.00f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    colors[ImGuiCol_TitleBg]                = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.00f, 0.48f, 0.80f, 1.00f); // Blue Accent Title Active
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    colors[ImGuiCol_CheckMark]              = ImVec4(0.00f, 0.90f, 0.46f, 1.00f); // Green Accent Checkmark
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.43f, 0.43f, 0.43f, 1.00f); // Medium Grey Slider
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_Button]                 = ImVec4(0.27f, 0.27f, 0.27f, 1.00f); // Medium Dark Grey Button
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
    colors[ImGuiCol_Header]                 = ImVec4(0.20f, 0.20f, 0.20f, 1.00f); // Dark Grey Header
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_Separator]              = colors[ImGuiCol_Border];
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.41f, 0.42f, 0.44f, 1.00f);
    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.51f, 0.53f, 0.55f, 1.00f);
    colors[ImGuiCol_ResizeGrip]             = ImVec4(0.31f, 0.31f, 0.31f, 0.20f);
    colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.41f, 0.41f, 0.41f, 0.67f);
    colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.51f, 0.51f, 0.51f, 0.95f);
    colors[ImGuiCol_Tab]                    = ImVec4(0.18f, 0.18f, 0.18f, 0.86f);
    colors[ImGuiCol_TabHovered]             = ImVec4(0.26f, 0.26f, 0.26f, 0.80f);
    colors[ImGuiCol_TabActive]              = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_TabUnfocused]           = ImVec4(0.15f, 0.15f, 0.15f, 0.97f);
    colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_PlotLines]              = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
    colors[ImGuiCol_TableBorderLight]       = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
    colors[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]          = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
    colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    colors[ImGuiCol_NavHighlight]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);

    // Style adjustments
    style.WindowRounding = 4.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.TabRounding = 4.0f;
}

void applyWhiteTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // Custom White Theme Colors
    colors[ImGuiCol_Text]                   = ImVec4(0.12f, 0.12f, 0.12f, 1.00f); // Very Dark Grey Text
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.96f, 0.96f, 0.96f, 1.00f); // Very Light Grey
    colors[ImGuiCol_ChildBg]                = ImVec4(1.00f, 1.00f, 1.00f, 1.00f); // White
    colors[ImGuiCol_PopupBg]                = ImVec4(1.00f, 1.00f, 1.00f, 0.98f);
    colors[ImGuiCol_Border]                 = ImVec4(0.74f, 0.74f, 0.74f, 0.50f); // Grey Border
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.91f, 0.91f, 0.91f, 1.00f); // Slightly Darker Light Grey Frame
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.85f, 0.85f, 0.85f, 1.00f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.78f, 0.78f, 0.78f, 1.00f);
    colors[ImGuiCol_TitleBg]                = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.75f, 0.75f, 0.75f, 1.00f); // Medium Grey Title Active
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(1.00f, 1.00f, 1.00f, 0.51f);
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.98f, 0.98f, 0.98f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.69f, 0.69f, 0.69f, 0.80f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.49f, 0.49f, 0.49f, 0.80f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.39f, 0.39f, 0.39f, 0.80f);
    colors[ImGuiCol_CheckMark]              = ImVec4(0.20f, 0.20f, 0.20f, 1.00f); // Dark Grey Checkmark
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.40f, 0.40f, 0.40f, 1.00f); // Medium Dark Grey Slider
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_Button]                 = ImVec4(0.86f, 0.86f, 0.86f, 1.00f); // Lighter Grey Button
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.78f, 0.78f, 0.78f, 1.00f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
    colors[ImGuiCol_Header]                 = ImVec4(0.88f, 0.88f, 0.88f, 1.00f); // Light Grey Header
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.75f, 0.75f, 0.75f, 1.00f);
    colors[ImGuiCol_Separator]              = colors[ImGuiCol_Border];
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.60f, 0.60f, 0.70f, 1.00f);
    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.70f, 0.70f, 0.90f, 1.00f);
    colors[ImGuiCol_ResizeGrip]             = ImVec4(0.80f, 0.80f, 0.80f, 0.56f);
    colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.70f, 0.70f, 0.70f, 0.78f);
    colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.60f, 0.60f, 0.60f, 0.95f);
    colors[ImGuiCol_Tab]                    = ImVec4(0.76f, 0.76f, 0.76f, 0.86f);
    colors[ImGuiCol_TabHovered]             = ImVec4(0.84f, 0.84f, 0.84f, 0.80f);
    colors[ImGuiCol_TabActive]              = ImVec4(0.88f, 0.88f, 0.88f, 1.00f);
    colors[ImGuiCol_TabUnfocused]           = ImVec4(0.92f, 0.92f, 0.92f, 0.97f);
    colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
    colors[ImGuiCol_PlotLines]              = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.45f, 0.00f, 1.00f);
    colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.78f, 0.78f, 0.78f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.57f, 0.57f, 0.64f, 1.00f);
    colors[ImGuiCol_TableBorderLight]       = ImVec4(0.68f, 0.68f, 0.74f, 1.00f);
    colors[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]          = ImVec4(0.30f, 0.30f, 0.30f, 0.09f);
    colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    colors[ImGuiCol_DragDropTarget]         = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    colors[ImGuiCol_NavHighlight]           = colors[ImGuiCol_HeaderHovered];
    colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(0.70f, 0.70f, 0.70f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.20f, 0.20f, 0.20f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);

    // Style adjustments
    style.WindowRounding = 4.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.TabRounding = 4.0f;
}
// --- End Theme Helper Functions ---

// --- Theme Setting Method Implementation (Issue #18) ---
void GuiInterface::setTheme(ThemeType theme) {
    switch (theme) {
        case ThemeType::DARK:
            applyDarkTheme();
            break;
        case ThemeType::WHITE:
            applyWhiteTheme();
            break;
        default:
            // Handle potential unknown theme type, maybe default to dark
            applyDarkTheme();
            break;
    }
}
// --- End Theme Setting Method Implementation ---
// --- Font Size Control Implementation (Issue #19) ---

// Helper function to load fonts with a specific size
void GuiInterface::loadFonts(float size) {
    ImGuiIO& io = ImGui::GetIO();
    // Ensure font data is available (basic check)
    if (!resources_NotoSans_Regular_ttf || resources_NotoSans_Regular_ttf_len == 0) {
         fprintf(stderr, "Error: Font resource data is missing or empty.\n");
         io.Fonts->AddFontDefault(); // Fallback
         fprintf(stderr, "Falling back to ImGui default font.\n");
         return;
    }

    ImFontConfig font_cfg;
    font_cfg.OversampleH = 2; // Improve rendering quality
    font_cfg.OversampleV = 1;
    font_cfg.PixelSnapH = true;
    font_cfg.FontDataOwnedByAtlas = false; // Font data is managed externally (in the header)

    // Load default ranges first (ASCII, basic Latin) from memory
    ImFont* font = io.Fonts->AddFontFromMemoryTTF(resources_NotoSans_Regular_ttf, (int)resources_NotoSans_Regular_ttf_len, size, &font_cfg, io.Fonts->GetGlyphRangesDefault());
    if (font == NULL) {
        fprintf(stderr, "Error: Failed to load default font segment from memory (size %.1f).\n", size);
        // Fall back to ImGui's default font if primary fails
        io.Fonts->AddFontDefault();
        fprintf(stderr, "Falling back to ImGui default font.\n");
        return; // Don't attempt merges if default load failed
    }

    // Define additional ranges (static to avoid redefinition)
    static const ImWchar extended_ranges[] =
    {
        0x0100, 0x017F, // Latin Extended-A
        0x0180, 0x024F, // Latin Extended-B
        0, // Null terminator
    };
    static const ImWchar cyrillic_ranges[] =
    {
        0x0400, 0x052F, // Cyrillic + Cyrillic Supplement
        0,
    };
    static const ImWchar emoji_ranges[] =
    {
        0x2600,  0x26FF,  // Miscellaneous Symbols
        0x2700,  0x27BF,  // Dingbats
        0,
    };

    // Merge additional ranges into the default font
    font_cfg.MergeMode = true; // Set MergeMode before the first merge

    // Merge Latin Extended A+B
    font = io.Fonts->AddFontFromMemoryTTF(resources_NotoSans_Regular_ttf, (int)resources_NotoSans_Regular_ttf_len, size, &font_cfg, extended_ranges);
    if (font == NULL) {
        fprintf(stderr, "Error: Failed to merge Latin Extended font segment (size %.1f).\n", size);
    }

    // Merge Cyrillic
    font = io.Fonts->AddFontFromMemoryTTF(resources_NotoSans_Regular_ttf, (int)resources_NotoSans_Regular_ttf_len, size, &font_cfg, cyrillic_ranges);
    if (font == NULL) {
        fprintf(stderr, "Error: Failed to merge Cyrillic font segment (size %.1f).\n", size);
    }

    // Merge Symbols
    font = io.Fonts->AddFontFromMemoryTTF(resources_NotoSans_Regular_ttf, (int)resources_NotoSans_Regular_ttf_len, size, &font_cfg, emoji_ranges);
    if (font == NULL) {
        fprintf(stderr, "Error: Failed to merge Symbols font segment (size %.1f).\n", size);
    }

    // IMPORTANT: Reset MergeMode only after the *last* merge operation
    font_cfg.MergeMode = false;

    // Note: io.Fonts->Build() is called in rebuildFontAtlas or initialize
}

// Rebuilds the font atlas with a new size
void GuiInterface::rebuildFontAtlas(float new_size) {
    // Ensure ImGui is initialized before proceeding
    if (!imgui_init_done) {
        fprintf(stderr, "Error: Attempted to rebuild font atlas before ImGui initialization.\n");
        return;
    }

    ImGuiIO& io = ImGui::GetIO();

    // Update the current font size state
    current_font_size = new_size;
    std::cout << "Rebuilding font atlas with size: " << current_font_size << std::endl;

    // 1. Destroy the existing GPU texture *before* we lose the handle
    if (io.Fonts->TexID)
        ImGui_ImplOpenGL3_DestroyFontsTexture();

    // 2. Clear existing fonts
    io.Fonts->Clear();

    // 3. Load fonts with the new size using the helper
    loadFonts(current_font_size);

    // 4. Build the new software-side font atlas
    if (!io.Fonts->Build()) {
        fprintf(stderr, "Error: Failed to build font atlas.\n");
        // Attempt to recover by adding default font
        io.Fonts->Clear();
        io.Fonts->AddFontDefault();
        io.Fonts->Build();
    }

    // 5. Create the new GPU texture
    if (!ImGui_ImplOpenGL3_CreateFontsTexture()) {
         fprintf(stderr, "Error: Failed to create GPU font texture.\n");
         // Consider how to handle this failure - maybe revert to a default?
    } else {
         std::cout << "Font atlas rebuilt and GPU texture updated successfully." << std::endl;
    }
}


// Method to reset font size to default (Issue #19 Fix)
void GuiInterface::resetFontSize() {
    // Call the private helper to rebuild with the default size (18.0f)
    rebuildFontAtlas(18.0f);
}


// Public method to change the font size
void GuiInterface::changeFontSize(float delta) {
    const float min_font_size = 8.0f;
    const float max_font_size = 72.0f;
    float desired_size = current_font_size + delta;

    // Clamp the desired size to the allowed range
    float clamped_size = std::clamp(desired_size, min_font_size, max_font_size);

    // Only rebuild if the clamped size is actually different
    if (std::abs(clamped_size - current_font_size) > 0.01f) { // Use epsilon for float comparison
        rebuildFontAtlas(clamped_size);
    } else {
         // Optional: Log if the size didn't change (e.g., already at min/max)
         // std::cout << "Font size change requested (" << delta << "), but size remains " << current_font_size << std::endl;
    }
}

// --- End Font Size Control Implementation ---
