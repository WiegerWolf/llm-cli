#pragma once

#include <GLFW/glfw3.h>

class GuiInterface;

namespace EventDispatch {

void custom_glfw_scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void glfw_error_callback(int error, const char* description);

} // namespace EventDispatch