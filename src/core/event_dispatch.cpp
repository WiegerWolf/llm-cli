#include <core/event_dispatch.h>
#include <gui/views/gui_interface.h>
#include <iostream>
#include <mutex>

namespace EventDispatch {

void custom_glfw_scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    GuiInterface* gui_ui = static_cast<GuiInterface*>(glfwGetWindowUserPointer(window));
    if (gui_ui) {
        std::lock_guard<std::mutex> lock(gui_ui->input_mutex);
        gui_ui->accumulated_scroll_x += static_cast<float>(xoffset);
        gui_ui->accumulated_scroll_y += static_cast<float>(yoffset);
    }
}

void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

} // namespace EventDispatch