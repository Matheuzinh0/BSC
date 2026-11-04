/**
 * =================================================================================================
 * Projeto: OmniStatement Studio
 * Desenvolvido por Matheus Sousa, Laboratório de Comunicação e Segurança da Informação,
 * Departamento de Eletrônica e Sistemas, CTG-UFPE.
 * =================================================================================================
 */

#pragma once

#include "ui/UIController.hpp"
#include <string>

struct GLFWwindow;

namespace ofx::ui {

class AppWindow {
public:
    AppWindow(int width, int height, const std::string& title, UIController& controller);
    ~AppWindow();

    bool init();
    void run();

private:
    static void dropCallback(GLFWwindow* window, int count, const char** paths);

    int m_width;
    int m_height;
    std::string m_title;
    UIController& m_controller;
    GLFWwindow* m_window = nullptr;
};

} // namespace ofx::ui
