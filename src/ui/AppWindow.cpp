/**
 * =================================================================================================
 * Projeto: BSC (Bank Statement Conversion)
 * Desenvolvido por Matheus Sousa, Laboratório de Comunicação e Segurança da Informação,
 * Departamento de Eletrônica e Sistemas, CTG-UFPE.
 * =================================================================================================
 */

#include "ui/AppWindow.hpp"
#include "ui/Theme.hpp"
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <iostream>

namespace ofx::ui {

AppWindow::AppWindow(int width, int height, const std::string& title, UIController& controller)
    : m_width(width), m_height(height), m_title(title), m_controller(controller) {}

AppWindow::~AppWindow() {
    if (m_window) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(m_window);
        glfwTerminate();
    }
}

void AppWindow::dropCallback(GLFWwindow* window, int count, const char** paths) {
    auto* self = static_cast<AppWindow*>(glfwGetWindowUserPointer(window));
    if (!self) return;

    std::vector<std::string> dropped;
    for (int i = 0; i < count; ++i) {
        dropped.push_back(paths[i]);
    }
    self->m_controller.addFiles(dropped);
}

bool AppWindow::init() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    // Try OpenGL 3.3 Core Profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    m_window = glfwCreateWindow(m_width, m_height, m_title.c_str(), nullptr, nullptr);
    if (!m_window) {
        // Fallback for legacy hardware: OpenGL 2.1 Compatibility Profile
        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
        m_window = glfwCreateWindow(m_width, m_height, m_title.c_str(), nullptr, nullptr);
        if (!m_window) {
            std::cerr << "Failed to create GLFW window with OpenGL 2.1/3.3 fallback" << std::endl;
            glfwTerminate();
            return false;
        }
    }

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1); // Enable V-Sync for energy efficiency on legacy laptops/CPUs

    glfwSetWindowUserPointer(m_window, this);
    glfwSetDropCallback(m_window, dropCallback);

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    loadApplicationFonts();
    applyModernDarkTheme();

    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    // Support legacy GLSL 1.20 for legacy GPUs and virtual environments
    const char* glsl_version = "#version 120";
    ImGui_ImplOpenGL3_Init(glsl_version);

    return true;
}

void AppWindow::run() {
    while (!glfwWindowShouldClose(m_window)) {
        // Wait events with timeout to keep CPU usage < 0.5% when idle
        glfwWaitEventsTimeout(0.016);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        m_controller.render();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(m_window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.06f, 0.09f, 0.16f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(m_window);
    }
}

} // namespace ofx::ui
