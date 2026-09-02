#pragma once

#include "imgui.h"
#include "ui/Anim.hpp"
#include "ui/Theme.hpp"

#include <cstdio>
#include <string>

namespace velora::ui::forms
{
inline void SectionTitle(const char* title)
{
    ImGui::Spacing();
    ImVec2 p = ImGui::GetCursorScreenPos();
    auto& th = Theme::get();
    ImGui::PushStyleColor(ImGuiCol_Text, th.accent);
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();
    ImVec2 a = ImGui::GetItemRectMin();
    ImVec2 b = ImGui::GetItemRectMax();
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImVec2(a.x, b.y + 2), ImVec2(a.x + 28, b.y + 4),
        ImGui::ColorConvertFloat4ToU32(th.accent), 2.f);
    ImGui::Spacing();
    ImGui::Dummy(ImVec2(0, 4));
}

/** Material You switch (animated thumb) */
inline bool Switch(const char* label, bool* v)
{
    ImGui::PushID(label);
    const float h = 28.f, w = 48.f;
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    bool pressed = ImGui::InvisibleButton("##sw", ImVec2(w, h));
    if (pressed) *v = !*v;

    /* smooth thumb pos stored in static map by id is hard; approximate with style */
    float t = *v ? 1.f : 0.f;
    auto& th = Theme::get();
    ImU32 track = *v ? ImGui::ColorConvertFloat4ToU32(ImVec4(th.accent.x, th.accent.y, th.accent.z, 0.55f))
                     : IM_COL32(255, 255, 255, 40);
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), track, h * 0.5f);
    float thumbX = p.x + 4.f + t * (w - h);
    dl->AddCircleFilled(ImVec2(thumbX + (h - 8) * 0.5f, p.y + h * 0.5f), (h - 10) * 0.5f,
                        IM_COL32(255, 255, 255, 245));
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::PopID();
    return pressed;
}

inline bool FilledButton(const char* label, const ImVec2& size = ImVec2(0, 0))
{
    auto& th = Theme::get();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(th.accent.x, th.accent.y, th.accent.z, 0.90f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(th.accent.x, th.accent.y, th.accent.z, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(th.accent.x * 0.85f, th.accent.y * 0.85f, th.accent.z * 0.85f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_Text, th.onPrimary);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 20.f);
    bool r = ImGui::Button(label, size);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);
    return r;
}

inline bool TonalButton(const char* label, const ImVec2& size = ImVec2(0, 0))
{
    auto& th = Theme::get();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(th.accent.x, th.accent.y, th.accent.z, 0.18f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(th.accent.x, th.accent.y, th.accent.z, 0.28f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(th.accent.x, th.accent.y, th.accent.z, 0.40f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 20.f);
    bool r = ImGui::Button(label, size);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    return r;
}

inline bool OutlinedButton(const char* label, const ImVec2& size = ImVec2(0, 0))
{
    /* No borders — tonal flat style */
    auto& th = Theme::get();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1, 1, 1, 0.06f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(th.accent.x, th.accent.y, th.accent.z, 0.14f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(th.accent.x, th.accent.y, th.accent.z, 0.22f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 20.f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.f);
    bool r = ImGui::Button(label, size);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);
    return r;
}

inline bool SearchField(const char* id, char* buf, size_t n, const char* hint = "Search")
{
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 24.f);
    ImGui::SetNextItemWidth(-1);
    bool r = ImGui::InputTextWithHint(id, hint, buf, (int)n);
    ImGui::PopStyleVar();
    return r;
}

inline void MetricCard(const char* title, const char* value, const ImVec4& accent)
{
    ImGui::BeginGroup();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    if (w > 200) w = (w - 16) * 0.33f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + 80), IM_COL32(255, 255, 255, 14), 20.f);
    dl->AddRectFilled(ImVec2(p.x, p.y + 12), ImVec2(p.x + 4, p.y + 68),
                      ImGui::ColorConvertFloat4ToU32(accent), 2.f);
    ImGui::Dummy(ImVec2(w, 80));
    ImGui::SetCursorScreenPos(ImVec2(p.x + 16, p.y + 16));
    ImGui::TextDisabled("%s", title);
    ImGui::SetCursorScreenPos(ImVec2(p.x + 16, p.y + 40));
    ImGui::SetWindowFontScale(1.25f);
    ImGui::TextUnformatted(value);
    ImGui::SetWindowFontScale(1.f);
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + 88));
    ImGui::EndGroup();
}

inline void ProgressBar(float t, const char* label = nullptr)
{
    t = anim::clamp01(t);
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    const float h = 8.f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), IM_COL32(255, 255, 255, 28), 4.f);
    auto& th = Theme::get();
    dl->AddRectFilled(p, ImVec2(p.x + w * t, p.y + h),
                      ImGui::ColorConvertFloat4ToU32(th.accent), 4.f);
    ImGui::Dummy(ImVec2(w, h + 4));
    if (label) ImGui::TextDisabled("%s", label);
}

inline bool Chip(const char* label, bool selected)
{
    auto& th = Theme::get();
    if (selected)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(th.accent.x, th.accent.y, th.accent.z, 0.35f));
        ImGui::PushStyleColor(ImGuiCol_Text, th.accent);
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1, 1, 1, 0.06f));
        ImGui::PushStyleColor(ImGuiCol_Text, th.text);
    }
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 16.f);
    bool r = ImGui::Button(label);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
    return r;
}

inline void LabeledInput(const char* label, char* buf, size_t n)
{
    ImGui::TextDisabled("%s", label);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 16.f);
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText((std::string("##") + label).c_str(), buf, (int)n);
    ImGui::PopStyleVar();
}
} // namespace velora::ui::forms
