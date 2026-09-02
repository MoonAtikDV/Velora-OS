#pragma once

#include "core/Kernel.hpp"
#include "ui/WindowManager.hpp"

#include <map>
#include <set>
#include <string>
#include <vector>

struct GLFWwindow;

namespace velora::ui
{
struct DeskIcon
{
    std::string id;          /* app id OR "file:<path>" */
    int col = 1;
    int row = 1;
    bool isFile = false;     /* shortcut to VelFS path */
    std::string filePath;    /* virtual path if isFile */
    std::string label;       /* optional display name */
};

struct Toast
{
    std::string text;
    float life = 2.5f;
};

class Shell
{
public:
    explicit Shell(core::Kernel& kernel);
    ~Shell();

    bool init(const char* title = "VeloraOS");
    int run();

private:
    void frame();
    void drawDesktop(ImVec2 size);
    void drawTaskbar(ImVec2 size);
    void drawReagent(ImVec2 size);
    void drawIcons(ImVec2 size);
    void drawToasts(ImVec2 size);
    void drawNotificationPanel(ImVec2 size);
    void drawContextMenu();

    void openApp(const std::string& id);
    void pinTaskbar(const std::string& id);
    void unpinTaskbar(const std::string& id);
    void addDesktopIcon(const std::string& id, int col, int row);
    void addDesktopFileShortcut(const std::string& vpath, int col = -1, int row = -1);
    void openDesktopEntry(const DeskIcon& ic);
    void removeDesktopIcon(const std::string& id);
    void toast(const std::string& text);
    void bindAppBody(AppWindow& w);
    void saveSession();
    void loadSession();
    void drawPowerDialog(ImVec2 size);

    /* Facide (desktop) cell metrics */
    void cellMetrics(ImVec2 size, float& cellW, float& cellH, float& originX, float& originY) const;

    core::Kernel& kernel_;
    GLFWwindow* window_ = nullptr;
    WindowManager wm_;

    bool reagentOpen_ = false;
    char reagentSearch_[64]{};

    std::vector<DeskIcon> desktopIcons_;
    std::vector<std::string> taskbarPins_;
    std::set<std::string> selected_;

    bool selecting_ = false;
    ImVec2 selStart_{};
    ImVec2 selEnd_{};

    bool draggingIcons_ = false;
    ImVec2 dragStartMouse_{};
    struct DragOrigin { std::string id; int col; int row; };
    std::vector<DragOrigin> dragOrigins_;

    bool ctxOpen_ = false;
    ImVec2 ctxPos_{};
    std::string ctxTarget_;
    int ctxOpenFrame_ = -1;

    std::vector<Toast> toasts_;

    unsigned reagentTex_ = 0;
    int reagentTexW_ = 0, reagentTexH_ = 0;

    std::map<std::string, float> hoverAnim_; /* 0..1 glass hover */
    float reagentAnim_ = 0.f;
    float taskbarReveal_ = 0.f;
    float notifAnim_ = 0.f;
    float desktopDim_ = 0.f;
    bool quickSwitch_ = false;
    bool reagentHovered_ = false;
    bool notifHovered_ = false;
    bool ctxHovered_ = false;
    bool taskbarHovered_ = false;
    void applyLiveSettings();

    std::vector<std::string> recentApps_;
    bool powerDialog_ = false;
    int powerChoice_ = 0; /* 0 sleep 1 restart 2 shutdown */
    float sessionSaveTimer_ = 0.f;

    /* floating taskbar margins (px) — equal on left/right/bottom */
    static constexpr float kBarMargin = 18.0f;
    static constexpr float kBarRadius = 18.0f;
    static constexpr int kCols = 25;
    static constexpr int kRows = 10;
};
} // namespace velora::ui
