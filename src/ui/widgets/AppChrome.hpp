#pragma once

#include "imgui.h"
#include "ui/Theme.hpp"
#include "ui/Anim.hpp"

#include <cstdio>
#include <string>

namespace velora::ui::chrome
{
/** Full-bleed MD3 content background for app body */
inline void BeginSurface(const char* id = "##surface")
{
    auto& th = Theme::get();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(th.bgWindow.x * 0.92f, th.bgWindow.y * 0.92f,
                                                   th.bgWindow.z * 0.92f, 1.f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::BeginChild(id, ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleVar();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 16));
    ImGui::Dummy(ImVec2(0, 0));
}

inline void EndSurface()
{
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

inline void Toolbar(float height = 52.f)
{
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    auto& th = Theme::get();
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + height),
                      ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, 0.04f)), 0.f);
    dl->AddLine(ImVec2(p.x, p.y + height - 1), ImVec2(p.x + w, p.y + height - 1),
                IM_COL32(255, 255, 255, 18));
    ImGui::BeginChild("##tb", ImVec2(w, height), false);
    ImGui::SetCursorPos(ImVec2(12, (height - 28) * 0.5f));
}

inline void EndToolbar() { ImGui::EndChild(); }

inline void SidebarFrame(float width, float height = 0.f)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1, 1, 1, 0.03f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 20.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));
    ImGui::BeginChild("##side", ImVec2(width, height), true);
}

inline void EndSidebar()
{
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

inline bool NavItem(const char* label, bool selected)
{
    auto& th = Theme::get();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    const float h = 40.f;
    if (selected)
    {
        ImGui::GetWindowDrawList()->AddRectFilled(
            p, ImVec2(p.x + w, p.y + h),
            ImGui::ColorConvertFloat4ToU32(ImVec4(th.accent.x, th.accent.y, th.accent.z, 0.22f)), 20.f);
        ImGui::GetWindowDrawList()->AddRectFilled(
            p, ImVec2(p.x + 4, p.y + h),
            ImGui::ColorConvertFloat4ToU32(th.accent), 2.f);
    }
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(1, 1, 1, 0.06f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(1, 1, 1, 0.10f));
    bool r = ImGui::Selectable(label, selected, 0, ImVec2(w, h));
    ImGui::PopStyleColor(3);
    return r;
}

inline void StatusBar(const char* text)
{
    float h = 28.f;
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + avail.y - h);
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + w, p.y + h), IM_COL32(0, 0, 0, 40));
    ImGui::SetCursorScreenPos(ImVec2(p.x + 12, p.y + 6));
    ImGui::TextDisabled("%s", text);
}
} // namespace velora::ui::chrome
