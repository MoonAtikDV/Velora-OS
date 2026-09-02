#pragma once

#include "imgui.h"
#include <algorithm>
#include <cmath>

namespace velora::ui
{
/* Material You (Material Design 3) tonal shell theme */
struct Theme
{
    ImVec4 accent      = ImVec4(0.404f, 0.314f, 0.643f, 1.0f); /* primary from wallpaper */
    ImVec4 accentHover = ImVec4(0.50f, 0.40f, 0.78f, 1.0f);
    ImVec4 onPrimary   = ImVec4(1, 1, 1, 1);
    ImVec4 surface     = ImVec4(0.11f, 0.11f, 0.12f, 1.0f);
    ImVec4 surfaceContainer = ImVec4(0.16f, 0.16f, 0.18f, 0.94f);
    ImVec4 surfaceContainerHigh = ImVec4(0.20f, 0.20f, 0.22f, 0.96f);
    ImVec4 bgDesktop   = ImVec4(0.035f, 0.043f, 0.059f, 1.0f);
    ImVec4 bgPanel     = ImVec4(0.16f, 0.16f, 0.18f, 0.94f);
    ImVec4 bgWindow    = ImVec4(0.13f, 0.13f, 0.15f, 0.98f);
    ImVec4 bgTitle     = ImVec4(0.404f, 0.314f, 0.643f, 0.18f);
    ImVec4 text        = ImVec4(0.90f, 0.90f, 0.92f, 1.0f);
    ImVec4 textMuted   = ImVec4(0.70f, 0.70f, 0.73f, 1.0f);
    ImVec4 danger      = ImVec4(0.96f, 0.35f, 0.35f, 1.0f);
    ImVec4 selection   = ImVec4(0.404f, 0.314f, 0.643f, 0.28f);
    ImVec4 outline     = ImVec4(0.55f, 0.55f, 0.58f, 0.35f);

    float taskbarH  = 64.0f;   /* M3 dock height */
    float titlebarH = 48.0f;
    float rounding  = 28.0f;   /* M3 shape large */
    int wallpaper   = 1;
    bool accentFromWallpaper = true;
    float animSpeed = 1.f;

    static Theme& get()
    {
        static Theme t;
        return t;
    }

    ImVec4 wallpaperColor() const
    {
        return bgDesktop;
    }

    void applyImGuiStyle() const
    {
        ImGuiStyle& s = ImGui::GetStyle();
        s.WindowRounding = rounding;
        s.ChildRounding = 20.0f;
        s.FrameRounding = 20.0f;   /* pill-ish M3 */
        s.PopupRounding = 16.0f;
        s.ScrollbarRounding = 12.0f;
        s.GrabRounding = 12.0f;
        s.TabRounding = 16.0f;
        s.WindowBorderSize = 0.0f;
        s.FrameBorderSize = 0.0f;
        s.PopupBorderSize = 0.0f;
        s.ChildBorderSize = 0.0f;
        s.TabBorderSize = 0.0f;
        s.WindowPadding = ImVec2(16, 16);
        s.FramePadding = ImVec2(16, 10);
        s.ItemSpacing = ImVec2(10, 10);
        s.ItemInnerSpacing = ImVec2(8, 6);
        s.ScrollbarSize = 10.0f;
        s.GrabMinSize = 14.0f;

        ImVec4* c = s.Colors;
        c[ImGuiCol_Text] = text;
        c[ImGuiCol_TextDisabled] = textMuted;
        c[ImGuiCol_WindowBg] = bgWindow;
        c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_PopupBg] = surfaceContainerHigh;
        c[ImGuiCol_Border] = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_FrameBg] = ImVec4(1, 1, 1, 0.06f);
        c[ImGuiCol_FrameBgHovered] = ImVec4(accent.x, accent.y, accent.z, 0.14f);
        c[ImGuiCol_FrameBgActive] = ImVec4(accent.x, accent.y, accent.z, 0.22f);
        c[ImGuiCol_TitleBg] = bgTitle;
        c[ImGuiCol_TitleBgActive] = ImVec4(accent.x, accent.y, accent.z, 0.28f);
        c[ImGuiCol_TitleBgCollapsed] = bgTitle;
        c[ImGuiCol_MenuBarBg] = surfaceContainer;
        c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_ScrollbarGrab] = ImVec4(1, 1, 1, 0.18f);
        c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(1, 1, 1, 0.28f);
        c[ImGuiCol_ScrollbarGrabActive] = ImVec4(1, 1, 1, 0.40f);
        c[ImGuiCol_CheckMark] = accent;
        c[ImGuiCol_SliderGrab] = accent;
        c[ImGuiCol_SliderGrabActive] = accentHover;
        c[ImGuiCol_Button] = ImVec4(accent.x, accent.y, accent.z, 0.18f);
        c[ImGuiCol_ButtonHovered] = ImVec4(accent.x, accent.y, accent.z, 0.28f);
        c[ImGuiCol_ButtonActive] = ImVec4(accent.x, accent.y, accent.z, 0.40f);
        c[ImGuiCol_Header] = ImVec4(accent.x, accent.y, accent.z, 0.16f);
        c[ImGuiCol_HeaderHovered] = ImVec4(accent.x, accent.y, accent.z, 0.26f);
        c[ImGuiCol_HeaderActive] = ImVec4(accent.x, accent.y, accent.z, 0.36f);
        c[ImGuiCol_Separator] = ImVec4(1, 1, 1, 0.08f);
        c[ImGuiCol_ResizeGrip] = ImVec4(accent.x, accent.y, accent.z, 0.20f);
        c[ImGuiCol_ResizeGripHovered] = ImVec4(accent.x, accent.y, accent.z, 0.40f);
        c[ImGuiCol_ResizeGripActive] = ImVec4(accent.x, accent.y, accent.z, 0.60f);
        c[ImGuiCol_Tab] = surfaceContainer;
        c[ImGuiCol_TabHovered] = ImVec4(accent.x, accent.y, accent.z, 0.22f);
        c[ImGuiCol_TabActive] = ImVec4(accent.x, accent.y, accent.z, 0.30f);
        c[ImGuiCol_PlotHistogram] = accent;
        c[ImGuiCol_TextSelectedBg] = ImVec4(accent.x, accent.y, accent.z, 0.35f);
        c[ImGuiCol_NavHighlight] = accent;
    }
};
} // namespace velora::ui
