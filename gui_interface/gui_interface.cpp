#include <cmath>
#include <cstdio> // For fprintf
#include "gui_interface.h"
#include "font_utils.h"
#include "theme_utils.h"
#include "event_dispatch.h"
#include <atomic>  // For std::atomic message id counter
#include <chrono> // Timestamp utilities
#include <stdexcept>
#include <iostream> // For error reporting during init/shutdown
#include <algorithm> // For std::clamp (Issue #19)
#include "config.h" // For DEFAULT_MODEL_ID

// Include GUI library headers
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

 // Forward declaration
class GuiInterface;

namespace {
/* Thread-safe monotonic message id counter */
std::atomic<NodeIdType> g_message_id_counter{0};

inline NodeIdType getNextMessageId() {
    return g_message_id_counter.fetch_add(1, std::memory_order_relaxed);
}

/* Timestamp helper (milliseconds since epoch, matching HistoryMessage::timestamp type) */
inline std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> getCurrentTimestamp() {
    return std::chrono::time_point_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now());
}
} // anonymous namespace


// Flag to track if ImGui backends were successfully initialized
static bool imgui_init_done = false;

GuiInterface::GuiInterface(Database& db_manager) : db_manager_ref(db_manager) {
    // Initialize the input buffer
    input_buf[0] = '\0';
    // current_selected_model_id_in_ui will be initialized in initialize()
    // available_models_for_ui will be populated by updateModelsList
    // models_are_loading_in_ui defaults to false (atomic)
}

GuiInterface::~GuiInterface() {
    // Ensure shutdown is called, although it should be called explicitly
    if (window) {
        shutdown();
    }
}

void GuiInterface::initialize() {
    // Load selected model ID or set default
    std::optional<std::string> loaded_model_id_opt = db_manager_ref.loadSetting("selected_model_id");
    if (loaded_model_id_opt.has_value() && !loaded_model_id_opt.value().empty()) {
        this->current_selected_model_id_in_ui = loaded_model_id_opt.value();
    } else {
        this->current_selected_model_id_in_ui = DEFAULT_MODEL_ID;
        db_manager_ref.saveSetting("selected_model_id", this->current_selected_model_id_in_ui);
    }
    std::optional<std::string> loaded_theme_opt = db_manager_ref.loadSetting("theme");
    if (loaded_theme_opt.has_value()) {
        if (loaded_theme_opt.value() == "white") {
            this->current_theme = ThemeType::WHITE;
        } else {
            this->current_theme = ThemeType::DARK;
        }
    }

    std::optional<std::string> loaded_font_size_opt = db_manager_ref.loadSetting("font_size");
    if (loaded_font_size_opt.has_value()) {
        try {
            float size = std::stof(loaded_font_size_opt.value());
            app::gui::FontUtils::setInitialFontSize(*this, size);
        } catch (const std::exception& e) {
            // Corrupted value, use default
        }
    }


    glfwSetErrorCallback(EventDispatch::glfw_error_callback);
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
    glfwSetScrollCallback(window, EventDispatch::custom_glfw_scroll_callback);

    // --- Initialize ImGui ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls

    ThemeUtils::setTheme(*this, this->current_theme);

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


    // Load Fonts using the helper function
    app::gui::FontUtils::rebuildFontAtlas(*this, this->current_font_size);

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
    return input_buf;
}

size_t GuiInterface::getInputBufferSize() const {
    return INPUT_BUFFER_SIZE;
}

// --- Implementation of UserInterface contract (Thread-Safe Communication) ---

std::optional<std::string> GuiInterface::promptUserInput() {
    std::unique_lock<std::mutex> lock(input_mutex);

    input_cv.wait(lock, [this]{ return !input_queue.empty() || shutdown_requested.load(); });

    if (shutdown_requested.load()) {
        return std::nullopt;
    }

    if (!input_queue.empty()) {
        std::string input = std::move(input_queue.front());
        input_queue.pop();
        return input;
    }

    return std::nullopt;
}

void GuiInterface::displayOutput(const std::string& output, const std::string& model_id) {
    enqueueDisplayMessage(MessageType::LLM_RESPONSE, output, std::make_optional(model_id));
}

void GuiInterface::displayError(const std::string& error) {
    enqueueDisplayMessage(MessageType::ERROR, error);
}

void GuiInterface::displayStatus(const std::string& status) {
    enqueueDisplayMessage(MessageType::STATUS, status);
}
 
void GuiInterface::enqueueDisplayMessage(MessageType type,
                                         const std::string& content,
                                         const std::optional<std::string>& model_id) {
    NodeIdType id = getNextMessageId();
    auto now = getCurrentTimestamp();
    std::lock_guard<std::mutex> lock(display_mutex);
    display_queue.push({id,
                        type,
                        content,
                        model_id,
                        now,
                        kInvalidNodeId});
}
 
// --- Methods for GUI thread to interact with Worker thread ---

void GuiInterface::requestShutdown() {
    shutdown_requested = true;
    input_cv.notify_one();
}

void GuiInterface::sendInputToWorker(const std::string& input) {
    {
        std::lock_guard<std::mutex> lock(input_mutex);
        input_queue.push(input);
    }
    input_cv.notify_one();
}

// --- Method for GUI thread to process display updates ---

std::vector<HistoryMessage> GuiInterface::processDisplayQueue() {
    std::vector<HistoryMessage> transferred_messages;
    transferred_messages.reserve(display_queue.size());
    std::lock_guard<std::mutex> lock(display_mutex);

    while (!display_queue.empty()) {
        transferred_messages.push_back(std::move(display_queue.front()));
        display_queue.pop();
    }
    return transferred_messages;
}

ImVec2 GuiInterface::getAndClearScrollOffsets() {
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

void GuiInterface::setLoadingModelsState(bool isLoading) {
    models_are_loading_in_ui = isLoading;
}

void GuiInterface::updateModelsList(const std::vector<ModelData>& models) {
    std::lock_guard<std::mutex> lock(models_ui_mutex);
    available_models_for_ui.clear();
    for (const auto& model : models) {
        available_models_for_ui.push_back({model.id, model.name});
    }
}

std::vector<GuiInterface::ModelEntry> GuiInterface::getAvailableModelsForUI() const {
    std::lock_guard<std::mutex> lock(models_ui_mutex);
    return available_models_for_ui;
}

void GuiInterface::setSelectedModelInUI(const std::string& model_id) {
    std::lock_guard<std::mutex> lock(models_ui_mutex);
    current_selected_model_id_in_ui = model_id;
    db_manager_ref.saveSetting("selected_model_id", model_id);
}

std::string GuiInterface::getSelectedModelIdFromUI() const {
    std::lock_guard<std::mutex> lock(models_ui_mutex);
    return current_selected_model_id_in_ui;
}

bool GuiInterface::areModelsLoadingInUI() const {
    return models_are_loading_in_ui.load();
}

Database& GuiInterface::getDbManager() {
    return db_manager_ref;
}
