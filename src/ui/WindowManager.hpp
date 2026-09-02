#pragma once

#include "imgui.h"
#include "ui/Anim.hpp"

#include <functional>
#include <string>
#include <vector>

namespace velora::ui
{
enum class WState { Normal, Minimized, Maximized };

struct AppWindow
{
    std::string id;
    std::string title;
    ImVec2 pos{120, 80};
    ImVec2 size{760, 520};
    ImVec2 restorePos{};
    ImVec2 restoreSize{};
    WState state = WState::Normal;
    bool open = true;
    bool focused = false;
    int z = 0;
    int pid = 0;

    /* Animation state */
    float appear = 0.f;       /* 0→1 open */
    float closeT = 0.f;       /* 0→1 closing */
    bool closing = false;
    anim::Spring focusGlow;   /* 0/1 focused */
    ImVec2 animPos{};
    ImVec2 animSize{};
    bool animInit = false;
    bool resizing = false;
    int resizeMask = 0; /* 1L 2R 4T 8B */

    std::function<void()> drawBody;
};

class WindowManager
{
public:
    AppWindow* open(const std::string& id, const std::string& title, ImVec2 size = {760, 520});
    AppWindow* find(const std::string& id);
    void close(const std::string& id);
    void minimize(const std::string& id);
    void restore(const std::string& id);
    void maximize(const std::string& id, ImVec2 desktop, float taskbarH);
    void focus(const std::string& id);

    void drawAll(ImVec2 desktop, float taskbarH);
    bool anyMaximized() const;
    bool anyOpen() const;

    std::vector<AppWindow>& list() { return windows_; }
    const std::vector<AppWindow>& list() const { return windows_; }

private:
    std::vector<AppWindow> windows_;
    int zTop_ = 1;
    void drawOne(AppWindow& w, ImVec2 desktop, float taskbarH, float dt);
    void tickAnims(AppWindow& w, float dt);
};
} // namespace velora::ui
