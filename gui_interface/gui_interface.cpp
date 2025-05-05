#include <cstdio> // For fprintf
#include "gui_interface.h"
#include <stdexcept>
#include <iostream> // For error reporting during init/shutdown

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


    // Load Fonts: Use Noto Sans for better Unicode support
    // ImGuiIO& io = ImGui::GetIO(); // io is already defined above (line 72)
    float font_size = 18.0f;

    ImFontConfig font_cfg;
    font_cfg.OversampleH = 2; // Improve rendering quality
    font_cfg.OversampleV = 1;
    font_cfg.PixelSnapH = true;
    font_cfg.FontDataOwnedByAtlas = false; // Font data is managed externally (in the header)

    // Load default ranges first (ASCII, basic Latin) from memory
    ImFont* font = io.Fonts->AddFontFromMemoryTTF(resources_NotoSans_Regular_ttf, (int)resources_NotoSans_Regular_ttf_len, font_size, &font_cfg, io.Fonts->GetGlyphRangesDefault());
    if (font == NULL) {
        fprintf(stderr, "Error: Failed to load default font segment from memory.\n");
        // Fall back to ImGui's default font
        io.Fonts->AddFontDefault();
        fprintf(stderr, "Falling back to ImGui default font.\n");
    }

    // Merge additional ranges (Latin Extended A+B for broader European language support)
    // Add more ranges (e.g., Cyrillic, Greek) here if needed in the future.
    static const ImWchar extended_ranges[] =
    {
        0x0100, 0x017F, // Latin Extended-A
        0x0180, 0x024F, // Latin Extended-B
        0, // Null terminator
    };
    // Define additional ranges
    static const ImWchar cyrillic_ranges[] =
    {
        0x0400, 0x052F, // Cyrillic + Cyrillic Supplement
        0,
    };
    // Add common Symbol ranges. Note: AddFontFromMemoryTTF expects ImWchar (16-bit),
    // so high-code-point Emojis (0x1Fxxx) cannot be added this way directly.
    // Including only the ranges that fit within ImWchar.
    static const ImWchar emoji_ranges[] =
    {
        0x2600,  0x26FF,  // Miscellaneous Symbols
        0x2700,  0x27BF,  // Dingbats
        // Ranges like 0x1F300-0x1F5FF are > 0xFFFF and incompatible here.
        0,
    };

    // Merge additional ranges into the default font
    font_cfg.MergeMode = true; // Set MergeMode before the first merge

    // Merge Latin Extended A+B
    font = io.Fonts->AddFontFromMemoryTTF(resources_NotoSans_Regular_ttf, (int)resources_NotoSans_Regular_ttf_len, font_size, &font_cfg, extended_ranges);
    if (font == NULL) {
        fprintf(stderr, "Error: Failed to load Latin Extended font segment from memory.\n");
    }

    // Merge Cyrillic
    // font_cfg.MergeMode = true; // Still true from previous call
    font = io.Fonts->AddFontFromMemoryTTF(resources_NotoSans_Regular_ttf, (int)resources_NotoSans_Regular_ttf_len, font_size, &font_cfg, cyrillic_ranges);
    if (font == NULL) {
        fprintf(stderr, "Error: Failed to load Cyrillic font segment from memory.\n");
    }

    // Merge Symbols (using ImWchar ranges)
    // font_cfg.MergeMode = true; // Still true from previous call
    font = io.Fonts->AddFontFromMemoryTTF(resources_NotoSans_Regular_ttf, (int)resources_NotoSans_Regular_ttf_len, font_size, &font_cfg, emoji_ranges);
    if (font == NULL) {
        fprintf(stderr, "Error: Failed to load Symbols font segment from memory.\n");
    }

    // IMPORTANT: Reset MergeMode only after the *last* merge operation
    font_cfg.MergeMode = false;

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
    ImGui::StyleColorsDark();
    // Optional: Add custom dark theme adjustments here
}

void applyWhiteTheme() {
    ImGui::StyleColorsLight(); // Use ImGui's built-in light theme
    // Optional: Define a fully custom white theme here if needed
    // Example:
    // ImGuiStyle& style = ImGui::GetStyle();
    // style.Colors[ImGuiCol_WindowBg] = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
    // style.Colors[ImGuiCol_Text] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    // ... other color adjustments ...
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
