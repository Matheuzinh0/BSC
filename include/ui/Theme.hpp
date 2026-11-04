/**
 * =================================================================================================
 * Projeto: BSC (Bank Statement Conversion)
 * Desenvolvido por Matheus Sousa, Laboratório de Comunicação e Segurança da Informação,
 * Departamento de Eletrônica e Sistemas, CTG-UFPE.
 * =================================================================================================
 */

#pragma once

#include "imgui.h"

namespace ofx::ui {

inline void applyModernDarkTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    
    // Geometry & Rounding
    style.WindowRounding = 8.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 9.0f;
    style.GrabRounding = 5.0f;
    style.TabRounding = 6.0f;

    style.WindowPadding = ImVec2(14.0f, 14.0f);
    style.FramePadding = ImVec2(10.0f, 6.0f);
    style.ItemSpacing = ImVec2(10.0f, 8.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 6.0f);
    style.IndentSpacing = 20.0f;
    style.ScrollbarSize = 12.0f;

    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.TabBorderSize = 0.0f;

    // Modern Deep Slate & Cyan Palette
    ImVec4* colors = style.Colors;

    // Backgrounds
    colors[ImGuiCol_WindowBg]             = ImVec4(0.06f, 0.09f, 0.16f, 1.00f); // #0F172A
    colors[ImGuiCol_ChildBg]              = ImVec4(0.12f, 0.16f, 0.23f, 0.70f); // #1E293B
    colors[ImGuiCol_PopupBg]              = ImVec4(0.09f, 0.13f, 0.21f, 0.98f);
    colors[ImGuiCol_Border]               = ImVec4(0.20f, 0.27f, 0.38f, 0.60f); // #334155
    colors[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    // Text
    colors[ImGuiCol_Text]                 = ImVec4(0.95f, 0.96f, 0.98f, 1.00f); // #F8FAFC
    colors[ImGuiCol_TextDisabled]         = ImVec4(0.50f, 0.58f, 0.69f, 1.00f); // #64748B

    // Headers & Titles
    colors[ImGuiCol_TitleBg]              = ImVec4(0.09f, 0.13f, 0.21f, 1.00f);
    colors[ImGuiCol_TitleBgActive]        = ImVec4(0.12f, 0.16f, 0.23f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.06f, 0.09f, 0.16f, 0.75f);
    colors[ImGuiCol_MenuBarBg]            = ImVec4(0.09f, 0.13f, 0.21f, 1.00f);

    // Frame inputs
    colors[ImGuiCol_FrameBg]              = ImVec4(0.15f, 0.20f, 0.29f, 0.80f);
    colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.20f, 0.27f, 0.38f, 0.80f);
    colors[ImGuiCol_FrameBgActive]        = ImVec4(0.24f, 0.32f, 0.45f, 1.00f);

    // Buttons (Vibrant Cyan / Teal accents)
    colors[ImGuiCol_Button]               = ImVec4(0.04f, 0.52f, 0.89f, 0.85f); // #0EA5E9
    colors[ImGuiCol_ButtonHovered]        = ImVec4(0.02f, 0.65f, 0.95f, 1.00f); // #38BDF8
    colors[ImGuiCol_ButtonActive]         = ImVec4(0.01f, 0.45f, 0.75f, 1.00f);

    // Tabs
    colors[ImGuiCol_Tab]                  = ImVec4(0.12f, 0.16f, 0.23f, 1.00f);
    colors[ImGuiCol_TabHovered]           = ImVec4(0.04f, 0.52f, 0.89f, 0.80f);
    colors[ImGuiCol_TabSelected]          = ImVec4(0.04f, 0.52f, 0.89f, 1.00f);
    colors[ImGuiCol_TabDimmed]            = ImVec4(0.09f, 0.13f, 0.21f, 1.00f);
    colors[ImGuiCol_TabDimmedSelected]    = ImVec4(0.15f, 0.20f, 0.29f, 1.00f);

    // Tables & Headers
    colors[ImGuiCol_TableHeaderBg]        = ImVec4(0.15f, 0.20f, 0.29f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]    = ImVec4(0.24f, 0.32f, 0.45f, 1.00f);
    colors[ImGuiCol_TableBorderLight]     = ImVec4(0.18f, 0.24f, 0.33f, 0.60f);
    colors[ImGuiCol_TableRowBg]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]        = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);

    // Sliders & Checkmarks
    colors[ImGuiCol_CheckMark]            = ImVec4(0.06f, 0.72f, 0.51f, 1.00f); // Emerald #10B981
    colors[ImGuiCol_SliderGrab]           = ImVec4(0.04f, 0.52f, 0.89f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.06f, 0.72f, 0.51f, 1.00f);

    // Scrollbar
    colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.06f, 0.09f, 0.16f, 0.60f);
    colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.20f, 0.27f, 0.38f, 0.80f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.30f, 0.40f, 0.55f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.04f, 0.52f, 0.89f, 1.00f);

    // Headers & Selections
    colors[ImGuiCol_Header]               = ImVec4(0.15f, 0.20f, 0.29f, 0.80f);
    colors[ImGuiCol_HeaderHovered]        = ImVec4(0.20f, 0.27f, 0.38f, 0.80f);
    colors[ImGuiCol_HeaderActive]         = ImVec4(0.04f, 0.52f, 0.89f, 0.60f);
}

} // namespace ofx::ui
