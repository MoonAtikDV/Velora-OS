#include "ui/WindowManager.hpp"
#include "ui/Theme.hpp"
#include "ui/Icons.hpp"
#include "ui/Assets.hpp"
#include "ui/Anim.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace velora::ui
{
AppWindow* WindowManager::find(const std::string& id)
{
    for (auto& w : windows_)
        if (w.id == id) return &w;
    return nullptr;
}

AppWindow* WindowManager::open(const std::string& id, const std::string& title, ImVec2 size)
{
    if (auto* e = find(id))
    {
        e->open = true;
        e->closing = false;
        e->closeT = 0.f;
        if (e->state == WState::Minimized)
            e->state = WState::Normal;
        if (e->appear < 0.5f)
            e->appear = 0.f; /* replay open anim if was gone */
        focus(id);
        return e;
    }
    AppWindow w;
    w.id = id;
    w.title = title;
    w.size = size;
    w.pos = ImVec2(90.f + float(windows_.size() % 8) * 36.f,
                   70.f + float(windows_.size() % 8) * 28.f);
    w.animPos = w.pos;
    w.animSize = w.size;
    w.animInit = true;
    w.appear = 0.f;
    w.z = ++zTop_;
    w.open = true;
    w.focusGlow.snap(1.f);
    windows_.push_back(std::move(w));
    focus(id);
    return &windows_.back();
}

void WindowManager::close(const std::string& id)
{
    if (auto* w = find(id))
    {
        w->closing = true;
        /* keep open=true until anim finishes so draw still runs */
    }
}

void WindowManager::minimize(const std::string& id)
{
    if (auto* w = find(id))
    {
        w->state = WState::Minimized;
        w->open = false;
        w->closing = false;
    }
}

void WindowManager::restore(const std::string& id)
{
    if (auto* w = find(id))
    {
        if (w->state == WState::Maximized)
        {
            w->state = WState::Normal;
            w->pos = w->restorePos;
            w->size = w->restoreSize;
        }
        else
            w->state = WState::Normal;
        w->open = true;
        w->closing = false;
        w->closeT = 0.f;
        w->appear = 0.f;
        focus(id);
    }
}

void WindowManager::maximize(const std::string& id, ImVec2 desktop, float taskbarH)
{
    auto* w = find(id);
    if (!w) return;
    if (w->state == WState::Maximized)
    {
        w->state = WState::Normal;
        w->pos = w->restorePos;
        w->size = w->restoreSize;
    }
    else
    {
        w->restorePos = w->pos;
        w->restoreSize = w->size;
        w->state = WState::Maximized;
        w->pos = ImVec2(0, 0);
        w->size = ImVec2(desktop.x, desktop.y - taskbarH);
    }
    w->open = true;
    focus(id);
}

void WindowManager::focus(const std::string& id)
{
    for (auto& w : windows_)
    {
        w.focused = (w.id == id);
        w.focusGlow.setTarget(w.focused ? 1.f : 0.f);
    }
    if (auto* w = find(id))
        w->z = ++zTop_;
}

bool WindowManager::anyMaximized() const
{
    for (const auto& w : windows_)
        if (w.open && !w.closing && w.state == WState::Maximized) return true;
    return false;
}

bool WindowManager::anyOpen() const
{
    for (const auto& w : windows_)
        if (w.open && !w.closing) return true;
    return false;
}

void WindowManager::tickAnims(AppWindow& w, float dt)
{
    if (!w.animInit)
    {
        w.animPos = w.pos;
        w.animSize = w.size;
        w.animInit = true;
    }

    /* open */
    if (!w.closing && w.appear < 1.f)
        w.appear = std::min(1.f, w.appear + dt * 3.2f);

    /* close */
    if (w.closing)
        w.closeT = std::min(1.f, w.closeT + dt * 4.0f);

    /* spring layout toward logical pos/size */
    float lambda = (w.state == WState::Maximized) ? 14.f : 16.f;
    if (w.resizing)
    {
        w.animPos = w.pos;
        w.animSize = w.size;
    }
    else
    {
        w.animPos = anim::damp2(w.animPos, w.pos, lambda, dt);
        w.animSize = anim::damp2(w.animSize, w.size, lambda, dt);
    }
    w.focusGlow.tick(dt);
}

void WindowManager::drawAll(ImVec2 desktop, float taskbarH)
{
    const float dt = ImGui::GetIO().DeltaTime;

    /* remove finished closes */
    windows_.erase(std::remove_if(windows_.begin(), windows_.end(),
                                  [](const AppWindow& w) {
                                      return w.closing && w.closeT >= 1.f;
                                  }),
                   windows_.end());

    std::vector<AppWindow*> order;
    for (auto& w : windows_)
        if (w.open || w.closing) order.push_back(&w);
    std::sort(order.begin(), order.end(), [](AppWindow* a, AppWindow* b) { return a->z < b->z; });
    for (auto* w : order)
    {
        tickAnims(*w, dt);
        if (w->open || w->closing)
            drawOne(*w, desktop, taskbarH, dt);
    }
}

void WindowManager::drawOne(AppWindow& w, ImVec2 desktop, float taskbarH, float /*dt*/)
{
    auto& theme = Theme::get();
    const float titleH = theme.titlebarH;

    if (w.state == WState::Maximized && !w.closing)
    {
        w.pos = ImVec2(0, 0);
        w.size = ImVec2(desktop.x, desktop.y - taskbarH);
    }

    w.size.x = std::max(320.f, w.size.x);
    w.size.y = std::max(240.f, w.size.y);

    const float openE = anim::easeOutBack(w.appear);
    const float closeE = anim::easeInOutCubic(w.closeT);
    float scale = anim::lerp(0.92f, 1.f, openE);
    if (w.closing)
        scale = anim::lerp(1.f, 0.88f, closeE);
    float alpha = anim::lerp(0.f, 1.f, anim::easeOutQuint(w.appear));
    if (w.closing)
        alpha = anim::lerp(1.f, 0.f, closeE);

    ImVec2 drawSize(w.animSize.x * scale, w.animSize.y * scale);
    ImVec2 drawPos(w.animPos.x + (w.animSize.x - drawSize.x) * 0.5f,
                   w.animPos.y + (w.animSize.y - drawSize.y) * 0.5f);

    /* Shadow under window */
    ImDrawList* bg = ImGui::GetBackgroundDrawList();
    float glow = w.focusGlow.value;
    if (alpha > 0.05f && w.state != WState::Maximized)
    {
        for (int i = 4; i >= 1; --i)
        {
            float o = (float)i * 3.f;
            int a = (int)((10 + 8 * glow) * alpha);
            bg->AddRectFilled(ImVec2(drawPos.x - o, drawPos.y - o + 4),
                              ImVec2(drawPos.x + drawSize.x + o, drawPos.y + drawSize.y + o + 6),
                              IM_COL32(0, 0, 0, a), theme.rounding + o);
        }
    }

    ImGui::SetNextWindowPos(drawPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(drawSize, ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints(ImVec2(320, 240), ImVec2(desktop.x, desktop.y - taskbarH));
    ImGui::SetNextWindowBgAlpha(alpha * 0.98f);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoScrollbar;

    float rounding = (w.state == WState::Maximized) ? 0.f : theme.rounding;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, rounding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, theme.bgWindow);

    char winId[128];
    std::snprintf(winId, sizeof(winId), "%s###app_%s", w.title.c_str(), w.id.c_str());
    bool vis = true;
    if (!ImGui::Begin(winId, &vis, flags))
    {
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(4);
        return;
    }
    if (!vis && !w.closing)
        close(w.id);

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
        focus(w.id);

    if (w.state != WState::Maximized && !w.closing && w.appear > 0.95f)
    {
        /* only commit user drag when fully open */
        ImVec2 wp = ImGui::GetWindowPos();
        ImVec2 ws = ImGui::GetWindowSize();
        /* avoid fighting anim — only if mouse dragging title */
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();
    ImVec2 ws = ImGui::GetWindowSize();

    /* Title bar with focus accent */
    ImVec2 t0 = wp;
    ImVec2 t1(wp.x + ws.x, wp.y + titleH);
    ImVec4 titleCol = anim::lerp4(
        ImVec4(theme.surfaceContainer.x, theme.surfaceContainer.y, theme.surfaceContainer.z, 1.f),
        ImVec4(theme.accent.x, theme.accent.y, theme.accent.z, 0.28f),
        w.focusGlow.value);
    dl->AddRectFilled(t0, t1, ImGui::ColorConvertFloat4ToU32(titleCol), rounding,
                      ImDrawFlags_RoundCornersTop);

    /* Accent line under title when focused */
    if (w.focusGlow.value > 0.05f)
    {
        dl->AddRectFilled(ImVec2(t0.x, t1.y - 2), t1,
                          ImGui::ColorConvertFloat4ToU32(
                              ImVec4(theme.accent.x, theme.accent.y, theme.accent.z, 0.7f * w.focusGlow.value)));
    }

    ImGui::SetCursorScreenPos(t0);
    ImGui::InvisibleButton("##titledrag", ImVec2(ws.x - 130.f, titleH));
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0) && w.state != WState::Maximized)
    {
        ImVec2 d = ImGui::GetIO().MouseDelta;
        w.pos.x += d.x;
        w.pos.y += d.y;
        w.animPos = w.pos;
    }
    if (ImGui::IsItemDeactivated() && w.state != WState::Maximized)
    {
        /* Edge snap */
        const float margin = 12.f;
        if (w.pos.x < margin) w.pos.x = 0;
        if (w.pos.y < margin) w.pos.y = 0;
        if (w.pos.x + w.size.x > desktop.x - margin) w.pos.x = desktop.x - w.size.x;
        if (w.pos.y + w.size.y > desktop.y - taskbarH - margin)
            w.pos.y = std::max(0.f, desktop.y - taskbarH - w.size.y);
        /* Top edge → maximize */
        if (w.pos.y <= 4.f)
            maximize(w.id, desktop, taskbarH);
        w.animPos = w.pos;
    }

    ImGui::SetCursorScreenPos(ImVec2(wp.x + 20.f, wp.y + (titleH - ImGui::GetFontSize()) * 0.5f));
    ImGui::TextUnformatted(w.title.c_str());

    const float btn = 40.f;
    float cx = wp.x + ws.x - btn * 3 - 8.f;
    auto ctrl = [&](const char* id, const char* glyph, bool danger, auto&& fn) {
        ImGui::SetCursorScreenPos(ImVec2(cx, wp.y + (titleH - btn) * 0.5f));
        ImGui::PushID(id);
        if (ImGui::InvisibleButton("##c", ImVec2(btn, btn)))
            fn();
        ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
        float hov = ImGui::IsItemHovered() ? 1.f : 0.f;
        if (hov > 0.f)
            dl->AddRectFilled(a, b,
                              danger ? IM_COL32(232, 80, 80, (int)(200 * hov))
                                     : IM_COL32(255, 255, 255, (int)(28 + 20 * hov)),
                              20.f);
        if (Assets::get().fontIcons()) ImGui::PushFont(Assets::get().fontIcons());
        ImVec2 ts = ImGui::CalcTextSize(glyph);
        dl->AddText(ImVec2((a.x + b.x - ts.x) * 0.5f, (a.y + b.y - ts.y) * 0.5f),
                    IM_COL32(255, 255, 255, 230), glyph);
        if (Assets::get().fontIcons()) ImGui::PopFont();
        ImGui::PopID();
        cx += btn;
    };
    ctrl("min", icons::remove(), false, [&] { minimize(w.id); });
    ctrl("max", icons::crop_square(), false, [&] { maximize(w.id, desktop, taskbarH); });
    ctrl("cls", icons::close(), true, [&] { close(w.id); });

    /* Body */
    ImGui::SetCursorScreenPos(ImVec2(wp.x, wp.y + titleH));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 12));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, std::max(0.35f, alpha));
    ImGui::BeginChild("##body", ImVec2(ws.x, ws.y - titleH), false,
                      ImGuiWindowFlags_AlwaysUseWindowPadding);
    if (w.drawBody && alpha > 0.15f)
        w.drawBody();
    ImGui::EndChild();
    ImGui::PopStyleVar(2);

    /* Resize: hit-test borders (works even over child content) */
    if (w.state != WState::Maximized && !w.closing && w.appear > 0.95f)
    {
        ImGuiIO& io = ImGui::GetIO();
        const float grip = 6.f;
        ImVec2 mouse = io.MousePos;
        auto inside = [&](float x0, float y0, float x1, float y1) {
            return mouse.x >= x0 && mouse.x < x1 && mouse.y >= y0 && mouse.y < y1;
        };
        int mask = 0;
        if (inside(wp.x - 2, wp.y + grip, wp.x + grip, wp.y + ws.y - grip)) mask |= 1;
        if (inside(wp.x + ws.x - grip, wp.y + grip, wp.x + ws.x + 2, wp.y + ws.y - grip)) mask |= 2;
        if (inside(wp.x + grip, wp.y - 2, wp.x + ws.x - grip, wp.y + grip)) mask |= 4;
        if (inside(wp.x + grip, wp.y + ws.y - grip, wp.x + ws.x - grip, wp.y + ws.y + 2)) mask |= 8;
        if (inside(wp.x - 2, wp.y - 2, wp.x + grip, wp.y + grip)) mask |= 1 | 4;
        if (inside(wp.x + ws.x - grip, wp.y - 2, wp.x + ws.x + 2, wp.y + grip)) mask |= 2 | 4;
        if (inside(wp.x - 2, wp.y + ws.y - grip, wp.x + grip, wp.y + ws.y + 2)) mask |= 1 | 8;
        if (inside(wp.x + ws.x - grip, wp.y + ws.y - grip, wp.x + ws.x + 2, wp.y + ws.y + 2)) mask |= 2 | 8;

        if (!io.MouseDown[0])
        {
            w.resizing = false;
            w.resizeMask = 0;
        }
        else if (!w.resizing && mask && ImGui::IsMouseClicked(0) && !ImGui::IsAnyItemActive())
        {
            w.resizing = true;
            w.resizeMask = mask;
            focus(w.id);
        }

        if (w.resizing && io.MouseDown[0])
        {
            ImVec2 d = io.MouseDelta;
            int m = w.resizeMask;
            if (m & 1)
            {
                float nx = w.size.x - d.x;
                if (nx >= 320.f) { w.pos.x += d.x; w.size.x = nx; }
            }
            if (m & 2)
                w.size.x = std::max(320.f, w.size.x + d.x);
            if (m & 4)
            {
                float ny = w.size.y - d.y;
                if (ny >= 240.f) { w.pos.y += d.y; w.size.y = ny; }
            }
            if (m & 8)
                w.size.y = std::max(240.f, w.size.y + d.y);
            w.size.x = std::min(w.size.x, desktop.x);
            w.size.y = std::min(w.size.y, desktop.y - taskbarH);
            w.animPos = w.pos;
            w.animSize = w.size;
        }
    }
    else
    {
        w.resizing = false;
        w.resizeMask = 0;
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(4);
}
} // namespace velora::ui
