/**
 * =================================================================================================
 * Projeto: BSC (Bank Statement Conversion)
 * Desenvolvido por Matheus Sousa, Laboratório de Comunicação e Segurança da Informação,
 * Departamento de Eletrônica e Sistemas, CTG-UFPE.
 * =================================================================================================
 */

#pragma once

#include "imgui.h"
#include <string>
#include <vector>
#include <filesystem>

namespace ofx::ui {

struct AppFonts {
    ImFont* fontRegular = nullptr;
    ImFont* fontBold = nullptr;
    ImFont* fontHeader = nullptr;
    ImFont* fontSmall = nullptr;
};

inline AppFonts& getAppFonts() {
    static AppFonts fonts;
    return fonts;
}

inline void loadApplicationFonts() {
    ImGuiIO& io = ImGui::GetIO();
    
    // Candidates for modern, clean, highly-legible UI fonts
    const char* regularCandidates[] = {
        "/usr/share/fonts/google-carlito-fonts/Carlito-Regular.ttf",
        "/usr/share/fonts/liberation-sans-fonts/LiberationSans-Regular.ttf",
        "/usr/share/fonts/adwaita-sans-fonts/AdwaitaSans-Regular.ttf",
        "/usr/share/fonts/abattis-cantarell-fonts/Cantarell-Regular.otf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf",
        "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf",
        "C:\\Windows\\Fonts\\segoeui.ttf",
        "C:\\Windows\\Fonts\\arial.ttf",
        "/System/Library/Fonts/SFNS.ttf",
        "/System/Library/Fonts/Helvetica.ttc"
    };

    const char* boldCandidates[] = {
        "/usr/share/fonts/google-carlito-fonts/Carlito-Bold.ttf",
        "/usr/share/fonts/liberation-sans-fonts/LiberationSans-Bold.ttf",
        "/usr/share/fonts/abattis-cantarell-fonts/Cantarell-Bold.otf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
        "/usr/share/fonts/truetype/ubuntu/Ubuntu-B.ttf",
        "C:\\Windows\\Fonts\\segoeuib.ttf",
        "C:\\Windows\\Fonts\\arialbd.ttf"
    };

    std::string foundRegular;
    for (const char* path : regularCandidates) {
        if (std::filesystem::exists(path)) {
            foundRegular = path;
            break;
        }
    }

    std::string foundBold;
    for (const char* path : boldCandidates) {
        if (std::filesystem::exists(path)) {
            foundBold = path;
            break;
        }
    }

    AppFonts& f = getAppFonts();

    if (!foundRegular.empty()) {
        ImFontConfig cfg;
        cfg.OversampleH = 2;
        cfg.OversampleV = 1;
        cfg.RasterizerMultiply = 1.05f;

        // Latin glyph ranges covering standard ASCII + Portuguese accents (á, é, í, ó, ú, ç, ã, õ, etc.)
        static const ImWchar glyphRanges[] = {
            0x0020, 0x00FF, // Basic Latin + Latin Supplement
            0x2010, 0x205E, // General Punctuation
            0x20A0, 0x20CF, // Currency Symbols (R$, €, etc.)
            0
        };

        // 1. Regular Body Font (15.5px - high legibility)
        f.fontRegular = io.Fonts->AddFontFromFileTTF(foundRegular.c_str(), 15.5f, &cfg, glyphRanges);

        // 2. Bold / Section Title Font (17.5px)
        std::string boldPath = !foundBold.empty() ? foundBold : foundRegular;
        f.fontBold = io.Fonts->AddFontFromFileTTF(boldPath.c_str(), 17.5f, &cfg, glyphRanges);

        // 3. Header / KPI Big Number Font (21.0px)
        f.fontHeader = io.Fonts->AddFontFromFileTTF(boldPath.c_str(), 21.0f, &cfg, glyphRanges);

        // 4. Small / Tag / Hash Font (13.0px)
        f.fontSmall = io.Fonts->AddFontFromFileTTF(foundRegular.c_str(), 13.0f, &cfg, glyphRanges);

        io.FontDefault = f.fontRegular;
    } else {
        f.fontRegular = io.Fonts->AddFontDefault();
        f.fontBold = f.fontRegular;
        f.fontHeader = f.fontRegular;
        f.fontSmall = f.fontRegular;
    }
}

inline void applyModernDarkTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    
    // Geometry & Rounding - Modern sleek rounded corners
    style.WindowRounding    = 8.0f;
    style.ChildRounding     = 8.0f;
    style.FrameRounding     = 6.0f;
    style.PopupRounding     = 8.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding      = 5.0f;
    style.TabRounding       = 6.0f;

    style.WindowPadding     = ImVec2(16.0f, 16.0f);
    style.FramePadding      = ImVec2(12.0f, 7.0f);
    style.ItemSpacing       = ImVec2(10.0f, 9.0f);
    style.ItemInnerSpacing  = ImVec2(8.0f, 6.0f);
    style.IndentSpacing     = 20.0f;
    style.ScrollbarSize     = 11.0f;

    style.WindowBorderSize  = 1.0f;
    style.ChildBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;
    style.FrameBorderSize   = 1.0f;
    style.TabBorderSize     = 0.0f;

    // Modern Deep Navy Slate & Emerald Fintech Palette
    ImVec4* colors = style.Colors;

    // Backgrounds
    colors[ImGuiCol_WindowBg]             = ImVec4(0.06f, 0.08f, 0.12f, 1.00f); // #0F141F
    colors[ImGuiCol_ChildBg]              = ImVec4(0.09f, 0.13f, 0.19f, 0.90f); // #172130
    colors[ImGuiCol_PopupBg]              = ImVec4(0.08f, 0.11f, 0.17f, 0.98f);
    colors[ImGuiCol_Border]               = ImVec4(0.18f, 0.24f, 0.34f, 0.70f); // #2E3E57
    colors[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    // Text
    colors[ImGuiCol_Text]                 = ImVec4(0.96f, 0.97f, 0.99f, 1.00f); // #F8FAFC
    colors[ImGuiCol_TextDisabled]         = ImVec4(0.55f, 0.63f, 0.73f, 1.00f); // #8CA1BA

    // Headers & Titles
    colors[ImGuiCol_TitleBg]              = ImVec4(0.08f, 0.11f, 0.17f, 1.00f);
    colors[ImGuiCol_TitleBgActive]        = ImVec4(0.11f, 0.16f, 0.24f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.06f, 0.08f, 0.12f, 0.75f);
    colors[ImGuiCol_MenuBarBg]            = ImVec4(0.08f, 0.11f, 0.17f, 1.00f);

    // Frame inputs (Textboxes, Combos)
    colors[ImGuiCol_FrameBg]              = ImVec4(0.12f, 0.17f, 0.25f, 0.85f);
    colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.16f, 0.22f, 0.32f, 0.95f);
    colors[ImGuiCol_FrameBgActive]        = ImVec4(0.19f, 0.26f, 0.38f, 1.00f);

    // Buttons (Vibrant Cyan / Teal accents)
    colors[ImGuiCol_Button]               = ImVec4(0.08f, 0.44f, 0.72f, 0.85f); // #1470B8
    colors[ImGuiCol_ButtonHovered]        = ImVec4(0.10f, 0.54f, 0.88f, 1.00f); // #1A8AE0
    colors[ImGuiCol_ButtonActive]         = ImVec4(0.06f, 0.38f, 0.62f, 1.00f);

    // Tabs
    colors[ImGuiCol_Tab]                  = ImVec4(0.10f, 0.14f, 0.21f, 1.00f);
    colors[ImGuiCol_TabHovered]           = ImVec4(0.14f, 0.48f, 0.78f, 0.85f);
    colors[ImGuiCol_TabSelected]          = ImVec4(0.08f, 0.44f, 0.72f, 1.00f);
    colors[ImGuiCol_TabDimmed]            = ImVec4(0.07f, 0.10f, 0.15f, 1.00f);
    colors[ImGuiCol_TabDimmedSelected]    = ImVec4(0.12f, 0.17f, 0.25f, 1.00f);

    // Tables & Headers
    colors[ImGuiCol_TableHeaderBg]        = ImVec4(0.12f, 0.17f, 0.25f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]    = ImVec4(0.18f, 0.24f, 0.34f, 0.80f);
    colors[ImGuiCol_TableBorderLight]     = ImVec4(0.14f, 0.19f, 0.28f, 0.45f);
    colors[ImGuiCol_TableRowBg]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]        = ImVec4(1.00f, 1.00f, 1.00f, 0.025f);

    // Sliders & Checkmarks
    colors[ImGuiCol_CheckMark]            = ImVec4(0.10f, 0.80f, 0.58f, 1.00f); // Bright Emerald
    colors[ImGuiCol_SliderGrab]           = ImVec4(0.08f, 0.44f, 0.72f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.10f, 0.80f, 0.58f, 1.00f);

    // Scrollbar
    colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.06f, 0.08f, 0.12f, 0.50f);
    colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.18f, 0.24f, 0.34f, 0.75f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.24f, 0.32f, 0.45f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.08f, 0.44f, 0.72f, 1.00f);

    // Headers & Selections
    colors[ImGuiCol_Header]               = ImVec4(0.13f, 0.19f, 0.28f, 0.80f);
    colors[ImGuiCol_HeaderHovered]        = ImVec4(0.18f, 0.26f, 0.38f, 0.85f);
    colors[ImGuiCol_HeaderActive]         = ImVec4(0.08f, 0.44f, 0.72f, 0.90f);
}

} // namespace ofx::ui
