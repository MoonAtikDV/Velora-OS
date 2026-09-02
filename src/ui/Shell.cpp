#include "ui/Shell.hpp"
#include "ui/Assets.hpp"
#include "ui/Icons.hpp"
#include "ui/Package.hpp"
#include "ui/Theme.hpp"
#include "ui/Anim.hpp"
#include "ui/Cursor.hpp"
#include "ui/widgets/Forms.hpp"
#include "ui/widgets/AppChrome.hpp"
#include "gfx/Graphics.hpp"
#include "system/Sandbox.hpp"
#include "system/Session.hpp"
#include "system/Notifications.hpp"
#include "system/PythonRuntime.hpp"
#include "system/SettingsStore.hpp"
#include "apps/ClorEngine.hpp"
#include "apps/EmbeddedBrowser.hpp"
#include "apps/Lattice.hpp"
#include "apps/Nucleus.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "ui/GlCompat.hpp"
#include "ui/IconCache.hpp"
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>
#include <iostream>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <filesystem>

namespace fs = std::filesystem;

namespace velora::ui
{
Shell::Shell(core::Kernel& kernel)
    : kernel_(kernel)
{
}

Shell::~Shell()
{
    IconCache::get().shutdown();
    PackageManager::get().shutdown();
    Assets::get().shutdown();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    if (window_)
        glfwDestroyWindow(window_);
    glfwTerminate();
}

bool Shell::init(const char* title)
{
    if (!glfwInit())
        return false;

    /* Compatibility profile — works with MinGW + system opengl32 without GLAD */
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif

    GLFWmonitor* mon = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = mon ? glfwGetVideoMode(mon) : nullptr;
    if (mode)
    {
        glfwWindowHint(GLFW_RED_BITS, mode->redBits);
        glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
        glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
        glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
    }
    /* True fullscreen on primary monitor */
    window_ = glfwCreateWindow(mode ? mode->width : 1440, mode ? mode->height : 900, title, mon, nullptr);
    if (!window_)
    {
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(window_);
    /* VSync applied again from Settings after load */
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    Theme::get().applyImGuiStyle();
    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    auto& assets = Assets::get();
    assets.discover();
    assets.loadFonts();
    ImGui_ImplOpenGL3_DestroyFontsTexture();
    ImGui_ImplOpenGL3_CreateFontsTexture();

    /* Show window first so user does not see a frozen white window */
    glfwShowWindow(window_);
    glfwSwapBuffers(window_);

    std::cout << "[shell] loading wallpapers..." << std::endl;
    assets.loadWallpapers();
    assets.applyAccentFromWallpaper(Theme::get().wallpaper);
    std::cout << "[shell] scanning packages..." << std::endl;
    PackageManager::get().scan(assets.mediaRoot());
    Cursor::get().load(assets.mediaRoot());
    velora::gfx::Graphics::get().detect();
    {
        auto& S = velora::system::SettingsStore::get();
        velora::gfx::Graphics::get().setPreferred(
            S.graphicsBackend == 1 ? velora::gfx::Backend::Vulkan : velora::gfx::Backend::OpenGL);
        glfwSwapInterval(S.vsync ? 1 : 0);
    }
    velora::apps::EmbeddedBrowser::get().init(window_);
    std::cout << "[shell] ready" << std::endl;

    /* Reagent start icon */
    for (const char* p : {"ui/reagent.png", "../ui/reagent.png", "reagent.png"})
    {
        reagentTex_ = Assets::get().loadTextureFile(p, reagentTexW_, reagentTexH_);
        if (reagentTex_)
        {
            std::cout << "[shell] reagent icon: " << p << std::endl;
            break;
        }
    }
    if (!reagentTex_)
        std::cout << "[shell] ui/reagent.png not found" << std::endl;

    /* Facide — default, then session overlay */
    desktopIcons_.clear();
    taskbarPins_.clear();
    recentApps_.clear();
    int row = 1;
    for (const auto& pkg : PackageManager::get().all())
    {
        if (row <= kRows)
            desktopIcons_.push_back({pkg.id, 1, row++});
    }
    for (const char* id : {"settings", "files", "terminal", "calculator", "clor"})
        if (PackageManager::get().find(id))
            taskbarPins_.push_back(id);

    loadSession();
    kernel_.log("Shell session ready");
    velora::system::NotificationCenter::get().post(
        "Welcome", "VeloraOS is ready. Open the notification center from the tray.", "system",
        velora::system::NotifLevel::Success, 4.5f);
    return true;
}

void Shell::toast(const std::string& text)
{
    velora::system::NotificationCenter::get().post("VeloraOS", text, "system");
}

void Shell::pinTaskbar(const std::string& id)
{
    if (!PackageManager::get().find(id)) return;
    if (std::find(taskbarPins_.begin(), taskbarPins_.end(), id) == taskbarPins_.end())
    {
        taskbarPins_.push_back(id);
        toast("Pinned to taskbar");
        saveSession();
    }
}

void Shell::unpinTaskbar(const std::string& id)
{
    taskbarPins_.erase(std::remove(taskbarPins_.begin(), taskbarPins_.end(), id), taskbarPins_.end());
    toast("Unpinned from taskbar");
    saveSession();
}

void Shell::addDesktopIcon(const std::string& id, int col, int row)
{
    for (auto& d : desktopIcons_)
        if (d.id == id)
        {
            d.col = col;
            d.row = row;
            return;
        }
    desktopIcons_.push_back({id, col, row});
}

void Shell::addDesktopFileShortcut(const std::string& vpath, int col, int row)
{
    if (vpath.empty()) return;
    std::string id = "file:" + vpath;
    for (auto& d : desktopIcons_)
        if (d.id == id) { d.col = col > 0 ? col : d.col; d.row = row > 0 ? row : d.row; return; }

    int c = col, r = row;
    if (c < 1 || r < 1)
    {
        /* first free cell near top-left of used area */
        c = 2; r = 1;
        bool used = true;
        while (used && r < 40)
        {
            used = false;
            for (const auto& d : desktopIcons_)
                if (d.col == c && d.row == r) { used = true; break; }
            if (used) { r++; if (r > 10) { r = 1; c++; } }
        }
    }
    std::string name = vpath;
    auto slash = name.find_last_of('/');
    if (slash != std::string::npos) name = name.substr(slash + 1);
    DeskIcon ic;
    ic.id = id;
    ic.col = c;
    ic.row = r;
    ic.isFile = true;
    ic.filePath = vpath;
    ic.label = name;
    desktopIcons_.push_back(std::move(ic));
    toast(std::string("Shortcut: ") + name);
    saveSession();
}

void Shell::openDesktopEntry(const DeskIcon& ic)
{
    if (ic.isFile)
    {
        /* Open Lattice and point user — navigate via Lattice static path */
        openApp("files");
        /* Stash path for Lattice to pick up */
        velora::apps::Lattice::get().requestOpen(ic.filePath);
        toast(ic.label.empty() ? ic.filePath : ic.label);
        return;
    }
    openApp(ic.id);
}

void Shell::removeDesktopIcon(const std::string& id)
{
    desktopIcons_.erase(std::remove_if(desktopIcons_.begin(), desktopIcons_.end(),
                                       [&](const DeskIcon& d) { return d.id == id; }),
                        desktopIcons_.end());
}

void Shell::cellMetrics(ImVec2 size, float& cellW, float& cellH, float& originX, float& originY) const
{
    auto& theme = Theme::get();
    const float bottomReserve = theme.taskbarH + kBarMargin * 2.f + 8.f;
    /* Adaptive margins / density by resolution */
    float scale = std::clamp(size.x / 1280.f, 0.75f, 1.5f);
    originX = 8.f * scale;
    originY = 8.f * scale;
    const float gridW = size.x - originX * 2.f;
    const float gridH = size.y - bottomReserve - originY;
    cellW = gridW / float(kCols);
    cellH = gridH / float(kRows);
}

void Shell::bindAppBody(AppWindow& w)
{
    const std::string id = w.id;
    if (id == "system" || id == "taskmgr")
    {
        w.drawBody = [this, id]() {
            auto& k = kernel_;
            k.tick();
            using namespace forms;
            SectionTitle(id == "taskmgr" ? "Task Manager" : "System overview");
            char cpu[32], mem[48], up[32];
            std::snprintf(cpu, sizeof(cpu), "%.0f%%", k.cpuUsage());
            std::snprintf(mem, sizeof(mem), "%.0f / %.0f MB", k.memUsedMb(), k.memTotalMb());
            std::snprintf(up, sizeof(up), "%lld s", (long long)k.uptimeSec());
            MetricCard("CPU", cpu, Theme::get().accent);
            ImGui::SameLine();
            MetricCard("Memory", mem, Theme::get().accentHover);
            ImGui::SameLine();
            MetricCard("Uptime", up, Theme::get().selection);

            if (id == "system")
            {
                SectionTitle("Graphics");
                const auto& gi = velora::gfx::Graphics::get().info();
                ImGui::Text("Backend: %s", velora::gfx::Graphics::get().backendName());
                ImGui::TextWrapped("Renderer: %s", gi.renderer.c_str());
                ImGui::TextWrapped("Version: %s", gi.version.c_str());
                ImGui::Text("Vulkan loader: %s", gi.vulkanAvailable ? "available" : "not found");
                ImGui::Text("OpenGL: %s", gi.openglAvailable ? "active" : "no");
                SectionTitle("Power");
                if (FilledButton("Sleep", ImVec2(120, 40))) k.requestPower(core::PowerAction::Sleep);
                ImGui::SameLine();
                if (FilledButton("Restart", ImVec2(120, 40))) k.requestPower(core::PowerAction::Restart);
                ImGui::SameLine();
                if (FilledButton("Shut down", ImVec2(120, 40))) k.requestPower(core::PowerAction::Shutdown);
            }

            SectionTitle("Processes");
            if (ImGui::Button("Refresh")) k.refreshProcesses();
            ImGui::BeginChild("##procs", ImVec2(0, 0), true);
            if (ImGui::BeginTable("##pt", 7, ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_NoBordersInBody))
            {
                ImGui::TableSetupColumn("PID", ImGuiTableColumnFlags_WidthFixed, 56);
                ImGui::TableSetupColumn("PPID", ImGuiTableColumnFlags_WidthFixed, 48);
                ImGui::TableSetupColumn("Name");
                ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 80);
                ImGui::TableSetupColumn("CPU", ImGuiTableColumnFlags_WidthFixed, 50);
                ImGui::TableSetupColumn("Mem", ImGuiTableColumnFlags_WidthFixed, 56);
                ImGui::TableSetupColumn("Perms");
                ImGui::TableHeadersRow();
                for (const auto& p : k.processes())
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("%d", p.pid);
                    ImGui::TableNextColumn(); ImGui::Text("%d", p.ppid);
                    ImGui::TableNextColumn(); ImGui::Text("%s%s", p.name.c_str(), p.system ? " *" : "");
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(p.state.c_str());
                    ImGui::TableNextColumn(); ImGui::Text("%.1f", p.cpu);
                    ImGui::TableNextColumn(); ImGui::Text("%.0f", p.memMb);
                    ImGui::TableNextColumn();
                    {
                        std::string bits;
                        auto add = [&](core::Perm bit, const char* s) {
                            if (core::hasPerm(p.perms, bit)) {
                                if (!bits.empty()) bits += "|";
                                bits += s;
                            }
                        };
                        add(core::Perm::FilesRead, "r");
                        add(core::Perm::FilesWrite, "w");
                        add(core::Perm::Network, "n");
                        add(core::Perm::Notifications, "i");
                        add(core::Perm::ProcessKill, "k");
                        add(core::Perm::Power, "p");
                        add(core::Perm::System, "S");
                        if (bits.empty()) bits = "-";
                        ImGui::TextUnformatted(bits.c_str());
                    }
                    if (!p.system && ImGui::BeginPopupContextItem())
                    {
                        if (ImGui::MenuItem("End task"))
                        {
                            k.killProcess(p.pid);
                            if (auto* win = wm_.find(p.appId))
                                wm_.close(p.appId);
                        }
                        ImGui::EndPopup();
                    }
                }
                ImGui::EndTable();
            }
            ImGui::EndChild();
        };
    }
    else if (id == "settings")
    {
        w.drawBody = [this]() {
            static int section = 0;
            auto& theme = Theme::get();
            auto& S = velora::system::SettingsStore::get();
            using namespace forms;

            const char* items[] = {
                "System", "Personalization", "Display", "Sound", "Network",
                "Privacy", "Power", "Notifications", "Taskbar", "Facide",
                "Accessibility", "Storage", "Apps", "Permissions", "Developer", "About"
            };
            const int nSec = 16;

            /* Win11-style settings shell */
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.11f, 1.f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 16));
            ImGui::BeginChild("##nav", ImVec2(240, 0), false);
            ImGui::TextUnformatted("Settings");
            ImGui::TextDisabled("Home · VeloraOS");
            ImGui::Spacing();
            static char setSearch[64]{};
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 20.f);
            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextWithHint("##ssearch", "Find a setting", setSearch, sizeof(setSearch));
            ImGui::PopStyleVar();
            ImGui::Spacing();
            for (int i = 0; i < nSec; ++i)
            {
                if (setSearch[0])
                {
                    std::string n = items[i];
                    std::string q = setSearch;
                    for (auto& c : n) c = (char)tolower((unsigned char)c);
                    for (auto& c : q) c = (char)tolower((unsigned char)c);
                    if (n.find(q) == std::string::npos) continue;
                }
                if (velora::ui::chrome::NavItem(items[i], section == i)) section = i;
            }
            ImGui::EndChild();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor();
            ImGui::SameLine(0, 0);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.14f, 0.15f, 1.f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(28, 20));
            ImGui::BeginChild("##body", ImVec2(0, 0), false);

            auto settingsCard = [](const char* title) {
                ImGui::Spacing();
                ImGui::TextUnformatted(title);
                ImGui::PushStyleColor(ImGuiCol_Text, Theme::get().textMuted);
                ImGui::Separator();
                ImGui::PopStyleColor();
                ImGui::Spacing();
            };

            if (section == 0)
            {
                settingsCard("System");
                Switch("UI animations", &S.animations);
                ImGui::SliderFloat("Motion scale", &theme.animSpeed, 0.5f, 1.5f, "%.2f");
                Switch("Dock blur", &S.blurDock);
                Switch("Reduce motion", &S.reduceMotion);
                ImGui::SliderFloat("UI scale", &S.uiScale, 0.85f, 1.35f, "%.2f");
                ImGui::Text("Language");
                ImGui::RadioButton("English", &S.language, 0); ImGui::SameLine();
                ImGui::RadioButton("Русский", &S.language, 1);
                ImGui::Text("Taskbar height");
                if (ImGui::SliderFloat("##tbh", &theme.taskbarH, 48.f, 88.f, "%.0f px"))
                    theme.applyImGuiStyle();
                ImGui::Text("Window rounding");
                if (ImGui::SliderFloat("##rnd", &theme.rounding, 8.f, 36.f, "%.0f"))
                    theme.applyImGuiStyle();
                if (FilledButton("Apply style", ImVec2(160, 40)))
                    theme.applyImGuiStyle();
            }
            else if (section == 1)
            {
                SectionTitle("Personalization");
                Switch("Accent from wallpaper", &S.accentFromWallpaper);
                theme.accentFromWallpaper = S.accentFromWallpaper;
                ImGui::SliderFloat("Desktop icon scale", &S.iconSizeScale, 0.7f, 1.4f, "%.2f");
                Switch("Show desktop labels", &S.showDesktopLabels);
                ImGui::ColorButton("##accent", theme.accent, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker, ImVec2(48, 48));
                ImGui::SameLine(); ImGui::Text("Current accent");
                ImGui::Spacing();
                ImGui::TextUnformatted("Background");
                const auto& wps = Assets::get().wallpapers();
                float cell = 160.f;
                int cols = std::max(1, (int)(ImGui::GetContentRegionAvail().x / (cell + 12.f)));
                int col = 0;
                for (int i = 0; i < (int)wps.size(); ++i)
                {
                    if (col) ImGui::SameLine(0, 12);
                    ImGui::PushID(i);
                    ImGui::BeginGroup();
                    unsigned tex = wps[i].thumbTexture ? wps[i].thumbTexture : wps[i].texture;
                    ImVec2 p = ImGui::GetCursorScreenPos();
                    if (tex)
                        ImGui::Image((ImTextureID)(intptr_t)tex, ImVec2(cell, cell * 0.56f));
                    else
                        ImGui::Dummy(ImVec2(cell, cell * 0.56f));
                    bool sel = theme.wallpaper == i + 1;
                    if (sel)
                    {
                        ImDrawList* dl = ImGui::GetWindowDrawList();
                        dl->AddRect(p, ImVec2(p.x + cell, p.y + cell * 0.56f),
                                    ImGui::ColorConvertFloat4ToU32(theme.accent), 12.f, 0, 3.f);
                    }
                    if (ImGui::IsItemClicked())
                    {
                        theme.wallpaper = i + 1;
                        if (S.accentFromWallpaper)
                            Assets::get().applyAccentFromWallpaper(theme.wallpaper);
                        saveSession();
                    }
                    ImGui::TextDisabled("%s", sel ? "Current" : "Preview");
                    ImGui::EndGroup();
                    ImGui::PopID();
                    col = (col + 1) % cols;
                }
            }
            else if (section == 2)
            {
                SectionTitle("Display");
                const auto& gi = velora::gfx::Graphics::get().info();
                ImGui::Text("API: %s", velora::gfx::Graphics::get().backendName());
                ImGui::TextWrapped("%s", gi.renderer.c_str());
                ImGui::TextWrapped("%s", gi.version.c_str());
                Switch("VSync", &S.vsync);
                if (ImGui::IsItemDeactivatedAfterEdit())
                {
                    glfwMakeContextCurrent(window_);
                    glfwSwapInterval(S.vsync ? 1 : 0);
                    toast(S.vsync ? "VSync on" : "VSync off");
                }
                ImGui::TextUnformatted("Graphics API");
                if (ImGui::RadioButton("OpenGL", &S.graphicsBackend, 0))
                {
                    velora::gfx::Graphics::get().setPreferred(velora::gfx::Backend::OpenGL);
                    toast("OpenGL preferred");
                }
                ImGui::SameLine();
                if (ImGui::RadioButton("Vulkan", &S.graphicsBackend, 1))
                {
                    velora::gfx::Graphics::get().setPreferred(velora::gfx::Backend::Vulkan);
                    toast(velora::gfx::Graphics::get().info().vulkanAvailable
                        ? "Vulkan preferred (loader found)"
                        : "Vulkan loader not found — UI stays OpenGL");
                }
                ImGui::TextDisabled("Shell compositor: OpenGL · Preferred: %s",
                    velora::gfx::Graphics::get().preferredName());
                ImGui::SliderInt("FPS limit", &S.fpsLimit, 30, 240);
                Switch("Fullscreen", &S.fullscreen);
                ImGui::SliderFloat("Gamma", &S.gamma, 0.7f, 1.4f, "%.2f");
                ImGui::Text("Vulkan: %s", gi.vulkanAvailable ? "available" : "not found");
            }
            else if (section == 3)
            {
                SectionTitle("Sound");
                Switch("Mute", &S.mute);
                ImGui::BeginDisabled(S.mute);
                ImGui::SliderFloat("Master volume", &S.masterVolume, 0.f, 1.f, "%.2f");
                ImGui::SliderFloat("Notification volume", &S.notifVolume, 0.f, 1.f, "%.2f");
                Switch("Sound on notification", &S.soundOnNotif);
                ImGui::EndDisabled();
            }
            else if (section == 4)
            {
                SectionTitle("Network");
                Switch("Online services", &S.onlineServices);
                Switch("Icon cache CDN", &S.iconCacheOnline);
                ImGui::InputText("Proxy", S.proxy, sizeof(S.proxy));
                ImGui::TextDisabled("Used by IconCache and Clor sandbox networking.");
                ImGui::Separator();
                ImGui::TextUnformatted("Python");
                {
                    auto& py = velora::system::PythonRuntime::get();
                    ImGui::Text("%s", py.detect() ? py.interpreter().c_str() : "Not found on PATH");
                }
            }
            else if (section == 5)
            {
                SectionTitle("Privacy");
                Switch("Telemetry (off by default)", &S.telemetry);
                ImGui::TextDisabled("Session auto-saves desktop, pins and preferences.");
            }
            else if (section == 6)
            {
                SectionTitle("Power");
                ImGui::SliderInt("Sleep after (min)", &S.sleepMinutes, 5, 120);
                Switch("Confirm shutdown", &S.confirmShutdown);
                if (FilledButton("Sleep", ImVec2(120, 40))) kernel_.requestPower(core::PowerAction::Sleep);
                ImGui::SameLine();
                if (FilledButton("Restart", ImVec2(120, 40))) kernel_.requestPower(core::PowerAction::Restart);
                ImGui::SameLine();
                if (FilledButton("Shut down", ImVec2(120, 40)))
                {
                    if (S.confirmShutdown) powerDialog_ = true;
                    else kernel_.requestPower(core::PowerAction::Shutdown);
                }
            }
            else if (section == 7)
            {
                SectionTitle("Notifications");
                Switch("Enable notifications", &S.notifEnabled);
                Switch("Show toasts", &S.notifToasts);
                Switch("Notification sounds", &S.notifSound);
                ImGui::SliderInt("Toast duration (sec)", &S.notifToastSec, 2, 15);
                if (FilledButton("Test notification", ImVec2(180, 40)))
                    velora::system::NotificationCenter::get().post("Settings", "Test notification", "settings",
                        velora::system::NotifLevel::Success, (float)S.notifToastSec);
                if (FilledButton("Open notification center", ImVec2(220, 40)))
                    velora::system::NotificationCenter::get().panelOpen = true;
            }
            else if (section == 8)
            {
                SectionTitle("Taskbar");
                Switch("Show labels", &S.taskbarLabels);
                Switch("Center icons", &S.taskbarCenter);
                Switch("Auto-hide", &S.taskbarAutohide);
                ImGui::SliderFloat("Opacity", &S.taskbarOpacity, 0.15f, 0.85f, "%.2f");
                ImGui::SliderFloat("Height", &theme.taskbarH, 48.f, 88.f, "%.0f");
            }
            else if (section == 9)
            {
                SectionTitle("Facide (Desktop)");
                Switch("Snap to grid", &S.snapToGrid);
                Switch("Show grid overlay", &S.showGrid);
                ImGui::SliderInt("Grid columns", &S.gridCols, 10, 40);
                ImGui::SliderInt("Grid rows", &S.gridRows, 6, 20);
                ImGui::TextDisabled("Drag files from Lattice onto Facide to create shortcuts.");
                if (FilledButton("Save desktop layout", ImVec2(200, 40)))
                {
                    saveSession();
                    toast("Desktop saved");
                }
            }
            else if (section == 10)
            {
                SectionTitle("Accessibility");
                Switch("High contrast", &S.highContrast);
                Switch("Large text", &S.largeText);
                ImGui::SliderFloat("Cursor size", &S.cursorSize, 0.8f, 2.f, "%.2f");
                if (S.largeText)
                    ImGui::SetWindowFontScale(1.15f);
                else
                    ImGui::SetWindowFontScale(1.f);
            }
            else if (section == 11)
            {
                SectionTitle("Storage");
                Switch("Auto-clean icon cache", &S.autoCleanCache);
                ImGui::SliderInt("Cache max (MB)", &S.cacheMaxMb, 64, 2048);
                ImGui::Text("VelFS root: %s", velora::fs::VelFS::get().hostRoot().c_str());
                if (FilledButton("Open Lattice", ImVec2(140, 40)))
                    openApp("files");
            }
            else if (section == 12)
            {
                SectionTitle("Apps");
                for (const auto& pkg : PackageManager::get().all())
                {
                    ImGui::BulletText("%s  ·  %s", pkg.name.c_str(), pkg.id.c_str());
                }
            }
            else if (section == 13)
            {
                SectionTitle("Permissions");
                ImGui::TextWrapped("Grant or revoke capabilities per application. System processes always keep full rights.");
                ImGui::Separator();
                for (const auto& pkg : PackageManager::get().all())
                {
                    ImGui::PushID(pkg.id.c_str());
                    ImGui::Text("%s (%s)", pkg.name.c_str(), pkg.id.c_str());
                    core::Perm m = core::AccessControl::get().mask(pkg.id);
                    bool fr = core::hasPerm(m, core::Perm::FilesRead);
                    bool fw = core::hasPerm(m, core::Perm::FilesWrite);
                    bool net = core::hasPerm(m, core::Perm::Network);
                    bool ntf = core::hasPerm(m, core::Perm::Notifications);
                    bool pwr = core::hasPerm(m, core::Perm::Power);
                    bool pkl = core::hasPerm(m, core::Perm::ProcessKill);
                    if (ImGui::Checkbox("files.read", &fr))
                    {
                        if (fr) core::AccessControl::get().grant(pkg.id, core::Perm::FilesRead);
                        else core::AccessControl::get().revoke(pkg.id, core::Perm::FilesRead);
                    }
                    ImGui::SameLine();
                    if (ImGui::Checkbox("files.write", &fw))
                    {
                        if (fw) core::AccessControl::get().grant(pkg.id, core::Perm::FilesWrite);
                        else core::AccessControl::get().revoke(pkg.id, core::Perm::FilesWrite);
                    }
                    ImGui::SameLine();
                    if (ImGui::Checkbox("network", &net))
                    {
                        if (net) core::AccessControl::get().grant(pkg.id, core::Perm::Network);
                        else core::AccessControl::get().revoke(pkg.id, core::Perm::Network);
                    }
                    if (ImGui::Checkbox("notifications", &ntf))
                    {
                        if (ntf) core::AccessControl::get().grant(pkg.id, core::Perm::Notifications);
                        else core::AccessControl::get().revoke(pkg.id, core::Perm::Notifications);
                    }
                    ImGui::SameLine();
                    if (ImGui::Checkbox("power", &pwr))
                    {
                        if (pwr) core::AccessControl::get().grant(pkg.id, core::Perm::Power);
                        else core::AccessControl::get().revoke(pkg.id, core::Perm::Power);
                    }
                    ImGui::SameLine();
                    if (ImGui::Checkbox("process.kill", &pkl))
                    {
                        if (pkl) core::AccessControl::get().grant(pkg.id, core::Perm::ProcessKill);
                        else core::AccessControl::get().revoke(pkg.id, core::Perm::ProcessKill);
                    }
                    /* sync running process mask */
                    if (auto* pr = kernel_.findByApp(pkg.id))
                        pr->perms = core::AccessControl::get().mask(pkg.id);
                    ImGui::Separator();
                    ImGui::PopID();
                }
            }
            else if (section == 14)
            {
                SectionTitle("Developer");
                Switch("Show FPS", &S.showFps);
                Switch("Debug bounds", &S.debugBounds);
                Switch("Verbose log", &S.verboseLog);
                if (FilledButton("Kernel log ping", ImVec2(160, 40)))
                    kernel_.log("Settings: developer ping");
            }
            else if (section == 15)
            {
                SectionTitle("About");
                ImGui::Text("VeloraOS %s", kernel_.version().c_str());
                ImGui::Text("Kernel: %s", kernel_.name().c_str());
                ImGui::TextWrapped("Native shell — GLFW, OpenGL, Dear ImGui, Material You.");
                ImGui::TextDisabled("Settings sections: %d", nSec);
            }

            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
        };
    }

    else if (id == "calculator")
    {
        w.drawBody = []() {
            static double acc = 0.0;
            static double current = 0.0;
            static char op = 0;
            static bool fresh = true;
            static bool error = false;
            static char display[48] = "0";

            auto setDisp = [&](double v) {
                if (std::fabs(v - std::floor(v)) < 1e-9)
                    std::snprintf(display, sizeof(display), "%.0f", v);
                else
                    std::snprintf(display, sizeof(display), "%.10g", v);
            };
            auto apply = [&]() {
                if (!op) { acc = current; return; }
                if (op == '+') acc = acc + current;
                else if (op == '-') acc = acc - current;
                else if (op == '*') acc = acc * current;
                else if (op == '/') {
                    if (std::fabs(current) < 1e-15) { error = true; std::snprintf(display, sizeof(display), "Error"); return; }
                    acc = acc / current;
                }
                current = acc;
                setDisp(current);
                fresh = true;
            };
            auto inputDigit = [&](int d) {
                if (error) { error = false; current = 0; acc = 0; op = 0; }
                if (fresh) { current = d; fresh = false; }
                else current = current * 10.0 + d;
                setDisp(current);
            };

            auto& th = Theme::get();
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 8));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 16.f);

            /* Display */
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0.25f));
            ImGui::BeginChild("##cdisp", ImVec2(-1, 72), false);
            ImGui::SetCursorPos(ImVec2(16, 18));
            float wrap = ImGui::GetContentRegionAvail().x - 8;
            ImVec2 ts = ImGui::CalcTextSize(display);
            ImGui::SetCursorPosX(std::max(16.f, wrap - ts.x + 16.f));
            ImGui::TextUnformatted(display);
            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::Spacing();

            const float gap = 8.f;
            float full = ImGui::GetContentRegionAvail().x;
            float bw = (full - gap * 3) / 4.f;
            float bh = 48.f;
            auto key = [&](const char* lab, auto fn, bool accent = false, bool wide = false) {
                ImVec2 sz(wide ? (bw * 2 + gap) : bw, bh);
                if (accent)
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(th.accent.x, th.accent.y, th.accent.z, 0.85f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(th.accent.x, th.accent.y, th.accent.z, 1.f));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
                }
                else
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1, 1, 1, 0.08f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.14f));
                    ImGui::PushStyleColor(ImGuiCol_Text, th.text);
                }
                if (ImGui::Button(lab, sz)) fn();
                ImGui::PopStyleColor(3);
            };

            key("C", [&]{ acc = current = 0; op = 0; fresh = true; error = false; setDisp(0); });
            ImGui::SameLine(0, gap);
            key("±", [&]{ current = -current; setDisp(current); });
            ImGui::SameLine(0, gap);
            key("%", [&]{ current *= 0.01; setDisp(current); });
            ImGui::SameLine(0, gap);
            key("÷", [&]{ apply(); op = '/'; fresh = true; }, true);

            key("7", [&]{ inputDigit(7); }); ImGui::SameLine(0, gap);
            key("8", [&]{ inputDigit(8); }); ImGui::SameLine(0, gap);
            key("9", [&]{ inputDigit(9); }); ImGui::SameLine(0, gap);
            key("×", [&]{ apply(); op = '*'; fresh = true; }, true);

            key("4", [&]{ inputDigit(4); }); ImGui::SameLine(0, gap);
            key("5", [&]{ inputDigit(5); }); ImGui::SameLine(0, gap);
            key("6", [&]{ inputDigit(6); }); ImGui::SameLine(0, gap);
            key("−", [&]{ apply(); op = '-'; fresh = true; }, true);

            key("1", [&]{ inputDigit(1); }); ImGui::SameLine(0, gap);
            key("2", [&]{ inputDigit(2); }); ImGui::SameLine(0, gap);
            key("3", [&]{ inputDigit(3); }); ImGui::SameLine(0, gap);
            key("+", [&]{ apply(); op = '+'; fresh = true; }, true);

            key("0", [&]{ inputDigit(0); }, false, true); ImGui::SameLine(0, gap);
            key(".", [&]{
                if (fresh) { current = 0; fresh = false; }
                /* simple: treat as visual only via string — keep integer mode for stability */
            });
            ImGui::SameLine(0, gap);
            key("=", [&]{ apply(); op = 0; fresh = true; }, true);

            ImGui::PopStyleVar(2);
        };
    }
    else if (id == "clock")
    {
        w.drawBody = []() {
            std::time_t t = std::time(nullptr);
            std::tm tm{};
#if defined(_MSC_VER)
            localtime_s(&tm, &t);
#else
            if (auto* p = std::localtime(&t)) tm = *p;
#endif
            ImGui::SetWindowFontScale(2.4f);
            ImGui::Text("%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
            ImGui::SetWindowFontScale(1.f);
            ImGui::Text("%02d.%02d.%04d", tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900);
        };
    }
    else if (id == "gallery")
    {
        w.drawBody = []() {
            const auto& wps = Assets::get().wallpapers();
            if (wps.empty())
            {
                ImGui::TextDisabled("No images in media/wallpapers/");
                return;
            }
            int cols = std::max(2, (int)(ImGui::GetContentRegionAvail().x / 160.f));
            int col = 0;
            for (int i = 0; i < (int)wps.size(); ++i)
            {
                if (col) ImGui::SameLine();
                unsigned tex = wps[i].thumbTexture ? wps[i].thumbTexture : wps[i].texture;
                if (tex) ImGui::Image((ImTextureID)(intptr_t)tex, ImVec2(148, 84));
                col = (col + 1) % cols;
            }
        };
    }

    else if (id == "files")
    {
        w.drawBody = []() { velora::apps::Lattice::get().draw(); };
    }
    else if (id == "terminal" || id == "logs")
    {
        w.drawBody = [this]() {
            velora::apps::Nucleus::get().draw(kernel_);
        };
    }
    else if (id == "notes")
    {
        w.drawBody = []() {
            static char buf[8192] =
                "Velora Notes\n\nType here. Content is local to this session.";
            ImGui::InputTextMultiline("##notes", buf, sizeof(buf), ImVec2(-1, -1));
        };
    }
    else if (id == "clor")
    {
        w.drawBody = [this]() {
            auto& clor = velora::apps::ClorEngine::get();
            auto& emb = velora::apps::EmbeddedBrowser::get();
            clor.ensureDefaults();
            static bool loadedBm = false;
            if (!loadedBm) { clor.loadPersistent(); loadedBm = true; }
            static char urlBuf[512] = "https://duckduckgo.com/";
            static bool urlSync = true;
            velora::apps::ClorTab* tab = clor.current();
            if (urlSync && tab)
            {
                std::snprintf(urlBuf, sizeof(urlBuf), "%s", tab->url.c_str());
                urlSync = false;
            }

            /* Tabs */
            for (int i = 0; i < (int)clor.tabs().size(); ++i)
            {
                if (i) ImGui::SameLine(0, 4);
                ImGui::PushID(i);
                bool active = (i == clor.active());
                if (active) ImGui::PushStyleColor(ImGuiCol_Button, Theme::get().accent);
                std::string label = clor.tabs()[i].title.empty() ? "New tab" : clor.tabs()[i].title;
                if (label.size() > 18) label = label.substr(0, 16) + "…";
                if (ImGui::Button(label.c_str(), ImVec2(0, 28)))
                {
                    clor.setActive(i);
                    urlSync = true;
                }
                if (active) ImGui::PopStyleColor();
                if (ImGui::BeginPopupContextItem())
                {
                    if (ImGui::MenuItem("Close tab")) { clor.closeTab(i); urlSync = true; }
                    if (ImGui::MenuItem("New tab")) { clor.newTab(); urlSync = true; }
                    ImGui::EndPopup();
                }
                ImGui::PopID();
            }
            ImGui::SameLine(0, 6);
            if (ImGui::Button("+", ImVec2(28, 28)))
            {
                clor.newTab("https://duckduckgo.com/");
                urlSync = true;
            }

            /* Nav bar */
            if (ImGui::Button("<", ImVec2(32, 32))) { clor.goBack(); emb.goBack(); urlSync = true; }
            ImGui::SameLine(0, 4);
            if (ImGui::Button(">", ImVec2(32, 32))) { clor.goForward(); emb.goForward(); urlSync = true; }
            ImGui::SameLine(0, 4);
            if (ImGui::Button("R", ImVec2(32, 32))) { clor.reload(); emb.reload(); urlSync = true; }
            ImGui::SameLine(0, 4);
            if (ImGui::Button("⌂", ImVec2(32, 32)))
            {
                clor.navigate("https://duckduckgo.com/");
                emb.navigate("https://duckduckgo.com/");
                urlSync = true;
            }
            ImGui::SameLine(0, 8);
            ImGui::SetNextItemWidth(-120);
            bool go = ImGui::InputText("##clorurl", urlBuf, sizeof(urlBuf), ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::SameLine();
            if (ImGui::Button("Go", ImVec2(48, 32)) || go)
            {
                clor.navigate(urlBuf);
                emb.navigate(urlBuf);
                urlSync = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("★", ImVec2(32, 32)) && tab)
            {
                clor.bookmarks().push_back({tab->title, tab->url});
                clor.savePersistent();
            }

            if (!clor.bookmarks().empty())
            {
                ImGui::BeginChild("##bmb", ImVec2(0, 28), false, ImGuiWindowFlags_NoScrollbar);
                for (int bi = 0; bi < (int)clor.bookmarks().size(); ++bi)
                {
                    if (bi) ImGui::SameLine(0, 6);
                    ImGui::PushID(1000 + bi);
                    if (ImGui::SmallButton(clor.bookmarks()[bi].title.c_str()))
                    {
                        clor.navigate(clor.bookmarks()[bi].url);
                        urlSync = true;
                    }
                    ImGui::PopID();
                }
                ImGui::EndChild();
            }
            ImGui::Separator();

            ImGui::TextDisabled("%s", clor.status().c_str());
            emb.tick();
            ImVec2 avail = ImGui::GetContentRegionAvail();
            ImVec2 p0 = ImGui::GetCursorScreenPos();
            /* Map ImGui screen pos → GLFW client area for WebView2 */
            {
                int wx = 0, wy = 0;
                glfwGetWindowPos(window_, &wx, &wy);
                /* ImGui coords are already in window client space for fullscreen */
                emb.setBounds((int)p0.x, (int)p0.y, (int)avail.x, (int)avail.y);
                emb.setVisible(true);
            }
            if (emb.available())
            {
                /* Leave empty hole for WebView2 HWND */
                ImGui::Dummy(avail);
                ImGui::SetCursorScreenPos(p0);
                ImGui::TextDisabled("Clor · WebView2 (in-OS)");
            }
            ImGui::BeginChild("##clorpage", avail, false);
            if (tab)
            {
                ImGui::TextUnformatted(tab->title.c_str());
                ImGui::TextDisabled("%s", tab->url.c_str());
                ImGui::Spacing();
                if (tab->loading)
                    ImGui::TextUnformatted("Loading…");
                if (!tab->pageError.empty())
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.4f, 0.4f, 1));
                    ImGui::TextWrapped("%s", tab->pageError.c_str());
                    ImGui::PopStyleColor();
                }
                if (!tab->pageText.empty())
                {
                    ImGui::PushTextWrapPos(0.0f);
                    ImGui::TextUnformatted(tab->pageText.c_str());
                    ImGui::PopTextWrapPos();
                }
                else if (!tab->loading)
                {
                    ImGui::TextWrapped(
                        "Clor loads page text into this window and can open a sandboxed Edge/Chrome "
                        "app window for full rendering. Enter a URL and press Go.");
                    if (ImGui::Button("Open in external sandbox", ImVec2(240, 36)) && tab)
                        clor.openExternal(tab->url);
                }
            }
            ImGui::EndChild();
        };
    }
    
    else
    {
        const AppPackage* pkg = PackageManager::get().find(id);
        if (pkg && pkg->isPython())
        {
            w.drawBody = [id]() {
                const AppPackage* p = PackageManager::get().find(id);
                static std::string output;
                static std::string status = "Ready";
                static bool ran = false;
                auto& py = velora::system::PythonRuntime::get();

                ImGui::Text("Python package: %s", p ? p->name.c_str() : id.c_str());
                ImGui::TextDisabled("%s", p ? p->folderPath.c_str() : "");
                ImGui::Text("Interpreter: %s",
                            py.detect() ? py.interpreter().c_str() : "(not found)");
                ImGui::Separator();
                if (ImGui::Button("Run", ImVec2(100, 36)) || !ran)
                {
                    ran = true;
                    if (!p)
                    {
                        status = "package missing";
                    }
                    else if (!py.detect())
                    {
                        status = "Python 3 not on PATH";
                        output = "Install Python 3 and restart VeloraOS.\n";
                    }
                    else
                    {
                        std::string script = p->folderPath + "/" + p->entry;
                        status = "Running...";
                        auto res = py.runFile(script, p->folderPath);
                        status = res.ok ? ("Exit 0") : ("Exit " + std::to_string(res.exitCode));
                        output = res.output;
                        if (!res.error.empty())
                            output += "\n" + res.error;
                        velora::system::NotificationCenter::get().post(
                            p->name, res.ok ? "Finished successfully" : "Script failed",
                            id,
                            res.ok ? velora::system::NotifLevel::Success
                                   : velora::system::NotifLevel::Error);
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Re-run", ImVec2(100, 36)))
                    ran = false;
                ImGui::TextDisabled("%s", status.c_str());
                ImGui::BeginChild("##pyout", ImVec2(0, 0), true);
                ImGui::TextUnformatted(output.c_str());
                ImGui::EndChild();
            };
        }
        else
        {
            w.drawBody = [id]() {
                ImGui::Text("Package: %s", id.c_str());
                ImGui::TextWrapped("This package has no native activity bound yet.");
            };
        }
    }
}

void Shell::openApp(const std::string& id)
{
    const AppPackage* pkg = PackageManager::get().find(id);
    const std::string title = pkg ? pkg->name : id;
    const bool sysPkg = pkg ? pkg->system : false;

    if (auto* existing = wm_.find(id))
    {
        if (!existing->open || existing->state == WState::Minimized)
            wm_.restore(id);
        else
            wm_.focus(id);
        if (existing->pid > 0)
            kernel_.setCurrentPid(existing->pid);
        else
            existing->pid = kernel_.spawn(id, title, sysPkg);
        reagentOpen_ = false;
        return;
    }

    ImVec2 size{760, 520};
    if (id == "calculator") size = {380, 520};
    if (id == "clor") size = {960, 640};
    if (id == "files") size = {1000, 680};
    if (id == "terminal" || id == "logs") size = {900, 560};
    if (id == "clock") size = {320, 200};

    auto* w = wm_.open(id, title, size);
    if (w)
    {
        w->pid = kernel_.spawn(id, title, sysPkg);
        bindAppBody(*w);
        toast(std::string("PID ") + std::to_string(w->pid) + " · " + title);
    }
    reagentOpen_ = false;
    saveSession();
}

void Shell::drawIcons(ImVec2 size)
{
    auto& theme = Theme::get();
    float cellW, cellH, originX, originY;
    cellMetrics(size, cellW, cellH, originX, originY);
    const float dt = ImGui::GetIO().DeltaTime;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 winPos = ImGui::GetWindowPos();
    ImGuiIO& io = ImGui::GetIO();

    if (!draggingIcons_ && ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0) && !ImGui::IsAnyItemHovered())
    {
        if (!io.KeyCtrl)
            selected_.clear();
        selecting_ = true;
        selStart_ = io.MousePos;
        selEnd_ = selStart_;
        ctxOpen_ = false;
    }
    if (selecting_)
    {
        selEnd_ = io.MousePos;
        if (ImGui::IsMouseReleased(0))
        {
            selecting_ = false;
            ImVec2 a(std::min(selStart_.x, selEnd_.x), std::min(selStart_.y, selEnd_.y));
            ImVec2 b(std::max(selStart_.x, selEnd_.x), std::max(selStart_.y, selEnd_.y));
            for (const auto& d : desktopIcons_)
            {
                float x = winPos.x + originX + (d.col - 1) * cellW;
                float y = winPos.y + originY + (d.row - 1) * cellH;
                ImVec2 ia(x, y), ib(x + cellW - 4, y + cellH - 4);
                if (ib.x >= a.x && ia.x <= b.x && ib.y >= a.y && ia.y <= b.y)
                    selected_.insert(d.id);
            }
        }
        else
        {
            {
                ImVec2 sa(std::min(selStart_.x, selEnd_.x), std::min(selStart_.y, selEnd_.y));
                ImVec2 sb(std::max(selStart_.x, selEnd_.x), std::max(selStart_.y, selEnd_.y));
                dl->AddRectFilled(sa, sb, ImGui::ColorConvertFloat4ToU32(theme.selection), 12.f);
                dl->AddRect(sa, sb, ImGui::ColorConvertFloat4ToU32(
                    ImVec4(theme.accent.x, theme.accent.y, theme.accent.z, 0.55f)), 12.f, 0, 1.5f);
            }
        }
    }

    /* Drag selected icons across entire Facide grid */
    if (draggingIcons_)
    {
        if (ImGui::IsMouseDown(0))
        {
            const float dx = io.MousePos.x - dragStartMouse_.x;
            const float dy = io.MousePos.y - dragStartMouse_.y;
            int dCol = (int)std::lround(dx / cellW);
            int dRow = (int)std::lround(dy / cellH);
            for (const auto& o : dragOrigins_)
            {
                for (auto& d : desktopIcons_)
                {
                    if (d.id != o.id) continue;
                    d.col = std::max(1, std::min(kCols, o.col + dCol));
                    d.row = std::max(1, std::min(kRows, o.row + dRow));
                }
            }
        }
        else
        {
            /* Resolve overlaps — icons cannot stack on same cell */
            for (size_t i = 0; i < desktopIcons_.size(); ++i)
            {
                for (size_t j = i + 1; j < desktopIcons_.size(); ++j)
                {
                    if (desktopIcons_[i].col == desktopIcons_[j].col &&
                        desktopIcons_[i].row == desktopIcons_[j].row)
                    {
                        /* push j to next free cell */
                        int c = desktopIcons_[j].col, r = desktopIcons_[j].row + 1;
                        bool used = true;
                        while (used)
                        {
                            used = false;
                            for (size_t k = 0; k < desktopIcons_.size(); ++k)
                            {
                                if (k == j) continue;
                                if (desktopIcons_[k].col == c && desktopIcons_[k].row == r)
                                { used = true; break; }
                            }
                            if (used) { r++; if (r > kRows) { r = 1; c++; if (c > kCols) c = 1; } }
                        }
                        desktopIcons_[j].col = c;
                        desktopIcons_[j].row = r;
                    }
                }
            }
            draggingIcons_ = false;
            dragOrigins_.clear();
            saveSession();
        }
    }

    for (auto& d : desktopIcons_)
    {
        const AppPackage* pkg = PackageManager::get().find(d.id);
        if (!pkg && !d.isFile) continue;
        std::string displayName = d.isFile ? (d.label.empty() ? d.filePath : d.label)
                                           : (pkg ? pkg->name : d.id);

        const float x = winPos.x + originX + (d.col - 1) * cellW;
        const float y = winPos.y + originY + (d.row - 1) * cellH;
        const float cw = cellW - 4.f;
        const float ch = cellH - 4.f;

        ImGui::SetCursorScreenPos(ImVec2(x, y));
        ImGui::PushID(d.id.c_str());
        if (ImGui::InvisibleButton("##desk", ImVec2(cw, ch)))
        {
            if (io.KeyCtrl)
            {
                if (selected_.count(d.id)) selected_.erase(d.id);
                else selected_.insert(d.id);
            }
            else if (!selected_.count(d.id))
            {
                selected_.clear();
                selected_.insert(d.id);
            }
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
            openDesktopEntry(d);

        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0, 3.0f) && !draggingIcons_ && !selecting_)
        {
            if (!selected_.count(d.id))
            {
                selected_.clear();
                selected_.insert(d.id);
            }
            draggingIcons_ = true;
            dragStartMouse_ = io.MousePos;
            dragOrigins_.clear();
            for (const auto& ic : desktopIcons_)
                if (selected_.count(ic.id))
                    dragOrigins_.push_back({ic.id, ic.col, ic.row});
        }

        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
        {
            ctxOpen_ = true;
            ctxOpenFrame_ = ImGui::GetFrameCount();
            ctxPos_ = io.MousePos;
            ctxTarget_ = d.id;
            if (!selected_.count(d.id))
            {
                selected_.clear();
                selected_.insert(d.id);
            }
        }

        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
        {
            ImGui::SetDragDropPayload("APP_ID", d.id.c_str(), d.id.size() + 1);
            ImGui::Text("%s", displayName.c_str());
            ImGui::EndDragDropSource();
        }

        const bool sel = selected_.count(d.id) > 0;
        const bool hot = ImGui::IsItemHovered();
        float& ha = hoverAnim_[d.id];
        float target = (hot || sel) ? 1.f : 0.f;
        ha += (target - ha) * std::min(1.f, dt * 14.f);

        ImVec2 a = ImGui::GetItemRectMin();
        ImVec2 b = ImGui::GetItemRectMax();

        /*
         * Win7-style glass frame: soft layers around the cell edge only.
         * Center (icon area) stays clear — blur/glass does not cover the icon.
         */
        if (ha > 0.01f)
        {
            const float outer = 3.0f;
            ImVec2 fa(a.x - outer, a.y - outer);
            ImVec2 fb(b.x + outer, b.y + outer);
            /* soft outer glow (blurred edge feel) */
            dl->AddRectFilled(fa, fb, IM_COL32(255, 255, 255, (int)(10 * ha)), 8.f);
            dl->AddRectFilled(a, b, IM_COL32(180, 210, 255, (int)(28 * ha)), 6.f);
            /* glass plate */
            dl->AddRectFilled(a, b, IM_COL32(255, 255, 255, (int)(18 * ha)), 6.f);
            /* bright edge only */
            dl->AddRect(a, b, IM_COL32(255, 255, 255, (int)(70 * ha)), 12.f, 0, 1.5f);
            /* punch hole for icon — redraw dark nothing? leave icon drawn on top */
            if (sel)
                dl->AddRect(a, b, ImGui::ColorConvertFloat4ToU32(ImVec4(theme.accent.x, theme.accent.y, theme.accent.z, 0.65f * ha)), 12.f, 0, 1.5f);
        }

        const float iconSz = std::min(96.f, std::min(cw, ch) * 0.72f);
        ImVec2 ic((a.x + b.x - iconSz) * 0.5f, a.y + ch * 0.12f);
        if (!d.isFile && pkg && pkg->iconTex)
            dl->AddImage((ImTextureID)(intptr_t)pkg->iconTex, ic, ImVec2(ic.x + iconSz, ic.y + iconSz));
        else
        {
            ImU32 col = d.isFile ? IM_COL32(66, 133, 200, 230)
                                 : ImGui::ColorConvertFloat4ToU32(theme.accent);
            dl->AddRectFilled(ic, ImVec2(ic.x + iconSz, ic.y + iconSz), col, 12.f);
            if (d.isFile)
            {
                const char* mark = "F";
                ImVec2 ts = ImGui::CalcTextSize(mark);
                dl->AddText(ImVec2(ic.x + (iconSz - ts.x) * 0.5f, ic.y + (iconSz - ts.y) * 0.5f),
                            IM_COL32(255, 255, 255, 255), mark);
            }
        }

        ImVec2 ns = ImGui::CalcTextSize(displayName.c_str());
        float tx = (a.x + b.x - ns.x) * 0.5f;
        float ty = ic.y + iconSz + 6.f;
        dl->AddText(ImVec2(tx + 1, ty + 1), IM_COL32(0, 0, 0, 160), displayName.c_str());
        dl->AddText(ImVec2(tx, ty), ImGui::ColorConvertFloat4ToU32(theme.text), displayName.c_str());
        ImGui::PopID();
    }

    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !ImGui::IsAnyItemHovered())
    {
        ctxOpen_ = true;
        ctxOpenFrame_ = ImGui::GetFrameCount();
        ctxPos_ = io.MousePos;
        ctxTarget_ = "Facide";
    }
}

void Shell::drawContextMenu()
{
    if (!ctxOpen_) return;
    {
        /* Keep menu on-screen */
        ImVec2 disp = ImGui::GetIO().DisplaySize;
        ImVec2 p = ctxPos_;
        const float estW = 220.f, estH = 180.f;
        if (p.x + estW > disp.x) p.x = disp.x - estW - 8.f;
        if (p.y + estH > disp.y) p.y = disp.y - estH - 8.f;
        if (p.x < 8.f) p.x = 8.f;
        if (p.y < 8.f) p.y = 8.f;
        ImGui::SetNextWindowPos(p);
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 16.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.13f, 0.15f, 0.94f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    if (ImGui::Begin("##ctx", &ctxOpen_,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
    {
        ctxHovered_ = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
        if (ctxTarget_ == "desktop" || ctxTarget_ == "Facide")
        {
            if (ImGui::MenuItem("Refresh"))
            {
                toast("Facide refreshed");
                ctxOpen_ = false;
            }
        }
        else
        {
            auto targets = selected_;
            if (targets.empty())
                targets.insert(ctxTarget_);

            if (ImGui::MenuItem("Open"))
            {
                for (const auto& id : targets)
                {
                    for (const auto& ic : desktopIcons_)
                        if (ic.id == id) { openDesktopEntry(ic); break; }
                }
                ctxOpen_ = false;
            }
            if (ImGui::MenuItem("Pin to taskbar"))
            {
                for (const auto& id : targets)
                    pinTaskbar(id);
                ctxOpen_ = false;
            }
            if (ImGui::MenuItem("Unpin from taskbar"))
            {
                for (const auto& id : targets)
                    unpinTaskbar(id);
                ctxOpen_ = false;
            }
            if (ImGui::MenuItem("Remove from Facide"))
            {
                for (const auto& id : targets)
                    removeDesktopIcon(id);
                selected_.clear();
                ctxOpen_ = false;
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}

void Shell::drawReagent(ImVec2 size)
{
    const float dt = ImGui::GetIO().DeltaTime;
    float target = reagentOpen_ ? 1.f : 0.f;
    reagentAnim_ = anim::damp(reagentAnim_, target, 14.f, dt);
    if (reagentAnim_ < 0.01f && !reagentOpen_) return;

    const float ease = anim::easeOutCubic(std::min(1.f, reagentAnim_));
    const float scale = anim::lerp(0.88f, 1.f, ease);

    auto& theme = Theme::get();
    const float width = 440.f;
    const float height = 500.f;
    const float barBottom = size.y - kBarMargin;
    float baseY = barBottom - theme.taskbarH - 14.f - height;
    if (baseY < 12.f) baseY = 12.f;
    float dw = width * scale;
    float dh = height * scale;
    ImVec2 pos(kBarMargin + (width - dw) * 0.5f,
               baseY + (height - dh) + (1.f - ease) * 28.f);
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(dw, dh), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.97f * ease);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 28.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 14));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.16f, 0.16f, 0.18f, 0.96f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    ImGui::Begin("##reagent", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    reagentHovered_ = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

    ImGui::TextUnformatted("Reagent");
    ImGui::TextDisabled("F1 toggle · Ctrl+S save session");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##search", "Type here to search", reagentSearch_, sizeof(reagentSearch_));
    ImGui::Spacing();

    ImGui::BeginChild("##apps", ImVec2(0, 0), false);
    const float btnW = 88.f;
    const float btnH = 96.f;
    int cols = std::max(1, (int)(ImGui::GetContentRegionAvail().x / (btnW + 8.f)));
    int col = 0;
    std::string q = reagentSearch_;
    for (auto& c : q) c = (char)std::tolower((unsigned char)c);

    for (const auto& pkg : PackageManager::get().all())
    {
        std::string name = pkg.name;
        std::string id = pkg.id;
        std::string nl = name, il = id;
        for (auto& c : nl) c = (char)std::tolower((unsigned char)c);
        for (auto& c : il) c = (char)std::tolower((unsigned char)c);
        if (!q.empty() && nl.find(q) == std::string::npos && il.find(q) == std::string::npos)
            continue;

        if (col) ImGui::SameLine(0, 8);
        ImGui::PushID(pkg.id.c_str());
        ImGui::BeginGroup();
        ImVec2 cur = ImGui::GetCursorScreenPos();
        if (ImGui::InvisibleButton("##app", ImVec2(btnW, btnH)))
            openApp(pkg.id);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
        {
            ctxOpen_ = true;
            ctxOpenFrame_ = ImGui::GetFrameCount();
            ctxPos_ = ImGui::GetIO().MousePos;
            ctxTarget_ = pkg.id;
        }
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
        {
            ImGui::SetDragDropPayload("APP_ID", pkg.id.c_str(), pkg.id.size() + 1);
            ImGui::TextUnformatted(pkg.name.c_str());
            ImGui::EndDragDropSource();
        }
        ImDrawList* dl = ImGui::GetWindowDrawList();
        float ha = ImGui::IsItemHovered() ? 1.f : 0.f;
        if (ha > 0.f)
            dl->AddRectFilled(cur, ImVec2(cur.x + btnW, cur.y + btnH),
                              IM_COL32(255, 255, 255, (int)(28 * ha)), 16.f);
        float isz = 40.f;
        ImVec2 ic(cur.x + (btnW - isz) * 0.5f, cur.y + 12.f);
        if (pkg.iconTex)
            dl->AddImage((ImTextureID)(intptr_t)pkg.iconTex, ic, ImVec2(ic.x + isz, ic.y + isz));
        else
            dl->AddRectFilled(ic, ImVec2(ic.x + isz, ic.y + isz),
                              ImGui::ColorConvertFloat4ToU32(theme.accent), 12.f);
        ImVec2 ts = ImGui::CalcTextSize(pkg.name.c_str());
        float tx = cur.x + (btnW - std::min(ts.x, btnW - 4.f)) * 0.5f;
        dl->AddText(ImVec2(tx, cur.y + 58.f), ImGui::ColorConvertFloat4ToU32(theme.text), pkg.name.c_str());
        ImGui::EndGroup();
        ImGui::PopID();
        col = (col + 1) % cols;
    }
    ImGui::EndChild();
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);

    if (ImGui::IsKeyPressed(ImGuiKey_Escape))
        reagentOpen_ = false;
}

void Shell::drawDesktop(ImVec2 size)
{
    auto& theme = Theme::get();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(size);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::Begin("Facide", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav |
                     ImGuiWindowFlags_NoScrollWithMouse);

    ImDrawList* bg = ImGui::GetWindowDrawList();
    ImVec2 a = ImGui::GetWindowPos();
    ImVec2 b(a.x + size.x, a.y + size.y);
    if (const Wallpaper* wp = Assets::get().wallpaperAt(theme.wallpaper))
        bg->AddImage((ImTextureID)(intptr_t)wp->texture, a, b);
    else
        bg->AddRectFilled(a, b, ImGui::ColorConvertFloat4ToU32(theme.wallpaperColor()));

    /* Build stamp — right side, above taskbar */
    {
        const char* stamp = "VeloraOS AlphaBuild";
        ImVec2 ts = ImGui::CalcTextSize(stamp);
        float stampY = a.y + size.y - Theme::get().taskbarH - kBarMargin - ts.y - 14.f;
        float stampX = a.x + size.x - ts.x - 24.f;
        bg->AddText(ImVec2(stampX + 1, stampY + 1), IM_COL32(0, 0, 0, 100), stamp);
        bg->AddText(ImVec2(stampX, stampY), IM_COL32(255, 255, 255, 140), stamp);
    }

    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("APP_ID"))
        {
            std::string id((const char*)p->Data);
            float cellW, cellH, originX, originY;
            cellMetrics(size, cellW, cellH, originX, originY);
            ImVec2 mp = ImGui::GetIO().MousePos;
            int col = 1 + (int)((mp.x - originX) / cellW);
            int row = 1 + (int)((mp.y - originY) / cellH);
            col = std::max(1, std::min(kCols, col));
            row = std::max(1, std::min(kRows, row));
            addDesktopIcon(id, col, row);
            toast("Shortcut added to Facide");
        }
        ImGui::EndDragDropTarget();
    }

    if (desktopDim_ > 0.01f)
    {
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImGui::GetWindowPos(),
            ImVec2(ImGui::GetWindowPos().x + size.x, ImGui::GetWindowPos().y + size.y),
            IM_COL32(0, 0, 0, (int)(110 * desktopDim_)));
    }
    drawIcons(size);

    /* Drop from Lattice → Facide shortcut */
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("VELFS_PATH"))
        {
            const char* path = (const char*)payload->Data;
            if (path && path[0])
            {
                ImVec2 mouse = ImGui::GetIO().MousePos;
                float cellW, cellH, ox, oy;
                cellMetrics(size, cellW, cellH, ox, oy);
                int col = (int)((mouse.x - ox) / cellW) + 1;
                int row = (int)((mouse.y - oy) / cellH) + 1;
                if (col < 1) col = 1;
                if (row < 1) row = 1;
                addDesktopFileShortcut(path, col, row);
            }
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::End();
    ImGui::PopStyleColor();
}



void Shell::drawTaskbar(ImVec2 size)
{
    auto& theme = Theme::get();
    auto& S = velora::system::SettingsStore::get();
    const float dt = ImGui::GetIO().DeltaTime;
    taskbarReveal_ = std::min(1.f, taskbarReveal_ + dt * 2.5f);

    const bool docked = wm_.anyMaximized();
    float margin = docked ? 0.f : (kBarMargin * std::clamp(size.x / 1280.f, 0.7f, 1.4f));
    float h = theme.taskbarH * std::clamp(size.y / 800.f, 0.85f, 1.25f);
    float radius = docked ? 0.f : kBarRadius;
    ImVec2 barPos(margin, size.y - h - margin);
    ImVec2 barSize(size.x - margin * 2.f, h);
    ImVec2 barMax(barPos.x + barSize.x, barPos.y + barSize.y);

    const float slide = (1.f - taskbarReveal_) * (h + (docked ? 0.f : kBarMargin) + 8.f);
    barPos.y += slide;
    barMax.y += slide;

    const float barAlpha = S.taskbarOpacity * taskbarReveal_;

    ImGui::SetNextWindowPos(barPos);
    ImGui::SetNextWindowSize(barSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, radius);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.12f, 0.13f, barAlpha));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    ImGui::Begin("##taskbar", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav);
    taskbarHovered_ = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

    ImDrawList* dlBar = ImGui::GetWindowDrawList();
    if (S.blurDock)
    {
        if (const Wallpaper* wp = Assets::get().wallpaperAt(theme.wallpaper))
        {
            unsigned tex = wp->blurTexture ? wp->blurTexture : wp->texture;
            if (tex)
            {
                ImVec2 uv0(barPos.x / size.x, barPos.y / size.y);
                ImVec2 uv1(barMax.x / size.x, barMax.y / size.y);
                dlBar->AddImageRounded((ImTextureID)(intptr_t)tex, barPos, barMax, uv0, uv1,
                                       IM_COL32(255, 255, 255, (int)(255 * taskbarReveal_)), radius);
                dlBar->AddRectFilled(barPos, barMax, IM_COL32(30, 30, 32, (int)(95 * taskbarReveal_)), radius);
            }
            else
                dlBar->AddRectFilled(barPos, barMax, IM_COL32(30, 30, 32, (int)(140 * taskbarReveal_)), radius);
        }
        else
            dlBar->AddRectFilled(barPos, barMax, IM_COL32(30, 30, 32, (int)(140 * taskbarReveal_)), radius);
    }
    else
        dlBar->AddRectFilled(barPos, barMax, IM_COL32(30, 30, 32, (int)(160 * taskbarReveal_)), radius);

    const float btn = 40.f;
    const float padY = (barSize.y - btn) * 0.5f;
    ImGui::SetCursorPos(ImVec2(12.f, padY));

    if (ImGui::InvisibleButton("##reagentStart", ImVec2(btn + 8.f, btn)))
        reagentOpen_ = !reagentOpen_;
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
        float hover = ImGui::IsItemHovered() ? 1.f : 0.f;
        float pulse = 0.5f + 0.5f * std::sin((float)ImGui::GetTime() * 2.2f);
        if (hover > 0.f || reagentOpen_)
            dl->AddRectFilled(a, b, IM_COL32(255, 255, 255, (int)(28 + 20 * hover + 10 * pulse * (reagentOpen_ ? 1 : 0))), 12.f);
        if (reagentTex_)
        {
            float isz = 28.f;
            ImVec2 ic((a.x + b.x - isz) * 0.5f, (a.y + b.y - isz) * 0.5f);
            dl->AddImage((ImTextureID)(intptr_t)reagentTex_, ic, ImVec2(ic.x + isz, ic.y + isz));
        }
        else
            dl->AddText(ImVec2(a.x + 14, a.y + 10), IM_COL32(255, 255, 255, 220), "R");
    }

    ImGui::SameLine(0, 10);
    /* pinned + open apps */
    std::vector<std::string> shown = taskbarPins_;
    for (auto& w : wm_.list())
    {
        if (!w.open && w.state != WState::Minimized) continue;
        if (std::find(shown.begin(), shown.end(), w.id) == shown.end())
            shown.push_back(w.id);
    }
    for (const auto& id : shown)
    {
        const AppPackage* pkg = PackageManager::get().find(id);
        ImGui::PushID(id.c_str());
        if (ImGui::InvisibleButton("##pin", ImVec2(btn, btn)))
            openApp(id);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
        {
            ctxOpen_ = true;
            ctxOpenFrame_ = ImGui::GetFrameCount();
            ctxPos_ = ImGui::GetIO().MousePos;
            ctxTarget_ = id;
        }
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
        float& ha = hoverAnim_[id];
        float target = (ImGui::IsItemHovered() ? 1.f : 0.f);
        ha = anim::damp(ha, target, 14.f, dt);
        if (ha > 0.01f)
            dl->AddRectFilled(a, b, IM_COL32(255, 255, 255, (int)(22 + 28 * ha)), 12.f);
        auto* win = wm_.find(id);
        if (win && (win->open || win->state == WState::Minimized))
            dl->AddRectFilled(ImVec2(a.x + 12, b.y - 3), ImVec2(b.x - 12, b.y - 1),
                              ImGui::ColorConvertFloat4ToU32(theme.accent), 2.f);
        if (pkg && pkg->iconTex)
        {
            float isz = 26.f;
            ImVec2 ic((a.x + b.x - isz) * 0.5f, (a.y + b.y - isz) * 0.5f);
            dl->AddImage((ImTextureID)(intptr_t)pkg->iconTex, ic, ImVec2(ic.x + isz, ic.y + isz));
        }
        else
            dl->AddText(ImVec2(a.x + 12, a.y + 10), IM_COL32(255, 255, 255, 200), id.c_str());
        ImGui::PopID();
        ImGui::SameLine(0, 4);
    }

    /* Clock + control center + notifications */
    char clockBuf[32], dateBuf[32];
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#if defined(_MSC_VER)
    localtime_s(&tm, &t);
#else
    if (auto* p = std::localtime(&t)) tm = *p;
#endif
    std::snprintf(clockBuf, sizeof(clockBuf), "%02d:%02d", tm.tm_hour, tm.tm_min);
    std::snprintf(dateBuf, sizeof(dateBuf), "%02d.%02d", tm.tm_mday, tm.tm_mon + 1);

    auto& nc = velora::system::NotificationCenter::get();
    int unread = nc.unreadCount();
    char bellBuf[24];
    std::snprintf(bellBuf, sizeof(bellBuf), unread > 0 ? "N·%d" : "N", unread > 9 ? 9 : unread);

    ImVec2 clockSize = ImGui::CalcTextSize(clockBuf);
    ImVec2 dateSize = ImGui::CalcTextSize(dateBuf);
    float clockBlockW = std::max(clockSize.x, dateSize.x) + 16.f;
    float sideW = 44.f * 2 + clockBlockW + 24.f;
    ImGui::SameLine(ImGui::GetWindowWidth() - sideW);
    ImGui::SetCursorPosY(padY);

    if (ImGui::InvisibleButton("##ctrlcenter", ImVec2(44.f, btn)))
    {
        /* Control center = quick settings */
        openApp("settings");
        toast("Control center");
    }
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
        if (ImGui::IsItemHovered())
            dl->AddRectFilled(a, b, IM_COL32(255, 255, 255, 32), 12.f);
        dl->AddText(ImVec2(a.x + 12, a.y + 10), IM_COL32(255, 255, 255, 220), "CC");
    }
    ImGui::SameLine(0, 4);
    if (ImGui::InvisibleButton("##notifbell", ImVec2(44.f, btn)))
        nc.togglePanel();
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
        if (ImGui::IsItemHovered() || nc.panelOpen)
            dl->AddRectFilled(a, b, IM_COL32(255, 255, 255, 32), 12.f);
        dl->AddText(ImVec2(a.x + 10, a.y + 10), IM_COL32(255, 255, 255, 220), bellBuf);
    }
    ImGui::SameLine(0, 8);
    ImGui::SetCursorPosY((barSize.y - (clockSize.y + dateSize.y + 2.f)) * 0.5f);
    ImGui::BeginGroup();
    ImGui::TextUnformatted(clockBuf);
    ImGui::TextDisabled("%s", dateBuf);
    ImGui::EndGroup();

    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}

void Shell::drawToasts(ImVec2 size)
{
    auto& nc = velora::system::NotificationCenter::get();
    if (!velora::system::SettingsStore::get().notifToasts) return;
    nc.tick(ImGui::GetIO().DeltaTime);

    float y = size.y - Theme::get().taskbarH - kBarMargin * 2.f - 16.f;
    auto toasts = nc.activeToasts();
    for (int i = (int)toasts.size() - 1; i >= 0; --i)
    {
        const auto* n = toasts[i];
        if (!n) continue;
        float alpha = 1.f;
        if (n->ttl > 0.f)
            alpha = std::clamp(1.f - (n->age / n->ttl) * 0.35f, 0.55f, 1.f);
        ImVec4 accent = Theme::get().accent;
        if (n->level == velora::system::NotifLevel::Success) accent = ImVec4(0.30f, 0.75f, 0.45f, 1);
        if (n->level == velora::system::NotifLevel::Warning) accent = ImVec4(0.95f, 0.70f, 0.25f, 1);
        if (n->level == velora::system::NotifLevel::Error) accent = ImVec4(0.95f, 0.35f, 0.35f, 1);
        float slideIn = anim::clamp01(n->age * 4.f);
        float easeIn = anim::easeOutCubic(slideIn);
        ImGui::SetNextWindowPos(ImVec2(size.x - 360.f - 16.f + (1.f - easeIn) * 48.f, y - 88.f));
        ImGui::SetNextWindowSize(ImVec2(360.f, 0), ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 20.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 14));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.12f, 0.14f, 0.72f * alpha));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
        ImGui::Begin(("##toastn" + std::to_string(n->id)).c_str(), nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing |
                         ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 a = ImGui::GetWindowPos();
        dl->AddRectFilled(a, ImVec2(a.x + 4, a.y + ImGui::GetWindowSize().y),
                          ImGui::ColorConvertFloat4ToU32(accent), 2.f);
        ImGui::Indent(8);
        ImGui::TextUnformatted(n->title.c_str());
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::get().textMuted);
        ImGui::TextWrapped("%s", n->body.c_str());
        ImGui::PopStyleColor();
        ImGui::Unindent(8);
        ImGui::End();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        y -= 100.f;
    }
}

void Shell::drawNotificationPanel(ImVec2 size)
{
    auto& nc = velora::system::NotificationCenter::get();
    float target = nc.panelOpen ? 1.f : 0.f;
    notifAnim_ = anim::damp(notifAnim_, target, 14.f, ImGui::GetIO().DeltaTime);
    if (notifAnim_ < 0.02f && !nc.panelOpen) return;

    const float panelW = 380.f;
    const float panelH = std::min(size.y * 0.65f, 480.f);
    float ease = anim::easeOutCubic(std::min(1.f, notifAnim_));
    ImVec2 pos(size.x - panelW - 16.f + (1.f - ease) * 48.f,
               size.y - Theme::get().taskbarH - kBarMargin * 2.f - panelH - 12.f);

    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelW, panelH), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.97f * ease);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 24.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 16));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.12f, 0.14f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    ImGui::Begin("##notifpanel", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse);
    notifHovered_ = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

    ImGui::TextUnformatted("Notifications");
    ImGui::SameLine(panelW - 150);
    if (ImGui::SmallButton("Read all")) nc.markAllRead();
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear")) nc.dismissAll();
    ImGui::Separator();

    ImGui::BeginChild("##nlist", ImVec2(0, 0), false);
    auto items = nc.items();
    if (items.empty())
        ImGui::TextDisabled("You're all caught up");
    for (const auto& n : items)
    {
        ImGui::PushID((int)n.id);
        ImVec4 accent = Theme::get().accent;
        if (n.level == velora::system::NotifLevel::Success) accent = ImVec4(0.30f, 0.75f, 0.45f, 1);
        if (n.level == velora::system::NotifLevel::Warning) accent = ImVec4(0.95f, 0.70f, 0.25f, 1);
        if (n.level == velora::system::NotifLevel::Error) accent = ImVec4(0.95f, 0.35f, 0.35f, 1);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1, 1, 1, n.read ? 0.04f : 0.08f));
        ImGui::BeginChild("##card", ImVec2(-1, 78), false);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 a = ImGui::GetWindowPos();
        ImVec2 bs = ImGui::GetWindowSize();
        dl->AddRectFilled(a, ImVec2(a.x + 4, a.y + bs.y), ImGui::ColorConvertFloat4ToU32(accent), 2.f);
        ImGui::SetCursorPos(ImVec2(14, 8));
        ImGui::TextUnformatted(n.title.c_str());
        ImGui::SetCursorPos(ImVec2(14, 28));
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::get().textMuted);
        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + panelW - 60);
        ImGui::TextUnformatted(n.body.c_str());
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
        ImGui::SetCursorPos(ImVec2(14, 54));
        if (ImGui::SmallButton("Dismiss")) nc.dismiss(n.id);
        ImGui::SameLine();
        if (!n.read && ImGui::SmallButton("Read")) nc.markRead(n.id);
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0, 6));
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}

void Shell::saveSession()
{
    auto& S = velora::system::SettingsStore::get();
    auto& th = Theme::get();
    velora::system::SessionData d;
    d.wallpaper = th.wallpaper;
    d.taskbarH = th.taskbarH;
    d.rounding = th.rounding;
    d.taskbarOpacity = S.taskbarOpacity;
    d.uiScale = S.uiScale;
    d.language = S.language;
    d.animations = S.animations;
    d.blurDock = S.blurDock;
    d.accentFromWallpaper = S.accentFromWallpaper;
    d.vsync = S.vsync;
    /* graphicsBackend via settings only */
    d.showFps = S.showFps;
    d.notifEnabled = S.notifEnabled;
    d.pins = taskbarPins_;
    for (const auto& ic : desktopIcons_)
    {
        velora::system::DeskEntry e;
        e.id = ic.id;
        e.col = ic.col;
        e.row = ic.row;
        d.desktop.push_back(e);
    }
    velora::system::Session::get().save(d);
}

void Shell::loadSession()
{
    velora::system::SessionData d;
    if (!velora::system::Session::get().load(d))
        return;
    auto& S = velora::system::SettingsStore::get();
    auto& th = Theme::get();
    if (d.wallpaper > 0)
    {
        th.wallpaper = d.wallpaper;
        if (d.accentFromWallpaper)
            Assets::get().applyAccentFromWallpaper(d.wallpaper);
    }
    if (d.taskbarH > 40.f) th.taskbarH = d.taskbarH;
    if (d.rounding > 0.f) th.rounding = d.rounding;
    S.taskbarOpacity = d.taskbarOpacity;
    S.uiScale = d.uiScale;
    S.language = d.language;
    S.animations = d.animations;
    S.blurDock = d.blurDock;
    S.accentFromWallpaper = d.accentFromWallpaper;
    S.vsync = d.vsync;
    S.showFps = d.showFps;
    S.notifEnabled = d.notifEnabled;
    th.accentFromWallpaper = d.accentFromWallpaper;
    th.applyImGuiStyle();
    if (!d.pins.empty())
        taskbarPins_ = d.pins;
    if (!d.desktop.empty())
    {
        desktopIcons_.clear();
        for (const auto& e : d.desktop)
        {
            DeskIcon ic;
            ic.id = e.id;
            ic.col = e.col;
            ic.row = e.row;
            if (e.id.rfind("file:", 0) == 0)
            {
                ic.isFile = true;
                ic.filePath = e.id.substr(5);
                auto slash = ic.filePath.find_last_of('/');
                ic.label = slash == std::string::npos ? ic.filePath : ic.filePath.substr(slash + 1);
            }
            desktopIcons_.push_back(std::move(ic));
        }
    }
}


void Shell::drawPowerDialog(ImVec2 size)
{
    if (!powerDialog_) return;
    ImGui::OpenPopup("Power");
    ImVec2 center(size.x * 0.5f, size.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(360, 200), ImGuiCond_Always);
    if (ImGui::BeginPopupModal("Power", &powerDialog_, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
    {
        ImGui::TextUnformatted("Power options");
        ImGui::Spacing();
        if (ImGui::Button("Sleep", ImVec2(-1, 40)))
        {
            kernel_.requestPower(core::PowerAction::Sleep);
            powerDialog_ = false;
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::Button("Restart", ImVec2(-1, 40)))
        {
            kernel_.requestPower(core::PowerAction::Restart);
            powerDialog_ = false;
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::Button("Shut down", ImVec2(-1, 40)))
        {
            kernel_.requestPower(core::PowerAction::Shutdown);
            powerDialog_ = false;
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::Button("Cancel", ImVec2(-1, 36)))
        {
            powerDialog_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void Shell::applyLiveSettings()
{
    auto& S = velora::system::SettingsStore::get();
    auto& theme = Theme::get();
    theme.accentFromWallpaper = S.accentFromWallpaper;
    if (S.reduceMotion)
        theme.animSpeed = 0.5f;
    else if (S.animations)
        theme.animSpeed = 1.f;
    else
        theme.animSpeed = 0.f;
    static int lastVsync = -999;
    int vs = S.vsync ? 1 : 0;
    if (vs != lastVsync)
    {
        lastVsync = vs;
        glfwMakeContextCurrent(window_);
        glfwSwapInterval(vs);
        std::cout << "[gfx] VSync " << (vs ? "ON" : "OFF") << std::endl;
    }
    static int lastBackend = -1;
    if (S.graphicsBackend != lastBackend)
    {
        lastBackend = S.graphicsBackend;
        velora::gfx::Graphics::get().setPreferred(
            S.graphicsBackend == 1 ? velora::gfx::Backend::Vulkan : velora::gfx::Backend::OpenGL);
    }
}

void Shell::frame()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    int winW = 0, winH = 0, fbW = 0, fbH = 0;
    glfwGetWindowSize(window_, &winW, &winH);
    glfwGetFramebufferSize(window_, &fbW, &fbH);
    ImVec2 size((float)winW, (float)winH);

    ImGuiIO& io = ImGui::GetIO();
    /* Global shortcuts */
    if (ImGui::IsKeyPressed(ImGuiKey_Escape))
    {
        if (reagentOpen_) reagentOpen_ = false;
        else if (powerDialog_) powerDialog_ = false;
        else if (velora::system::NotificationCenter::get().panelOpen)
            velora::system::NotificationCenter::get().panelOpen = false;
        else if (ctxOpen_) ctxOpen_ = false;
    }
    /* Click outside closes transient UI */
    if (ImGui::IsMouseClicked(0))
    {
        if (reagentOpen_ && !reagentHovered_ && !taskbarHovered_)
            reagentOpen_ = false;
        if (velora::system::NotificationCenter::get().panelOpen && !notifHovered_ && !taskbarHovered_)
            velora::system::NotificationCenter::get().panelOpen = false;
        if (ctxOpen_ && !ctxHovered_)
            ctxOpen_ = false;
    }
    /* reset hover flags each frame (set true while drawing) */
    reagentHovered_ = notifHovered_ = ctxHovered_ = taskbarHovered_ = false;

    applyLiveSettings();
    if (ImGui::IsKeyPressed(ImGuiKey_F1))
        reagentOpen_ = !reagentOpen_;
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S))
    {
        saveSession();
        toast("Session saved");
    }
    if (io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_F4))
        glfwSetWindowShouldClose(window_, 1);
    if (io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_Tab))
        quickSwitch_ = true;
    if (!io.KeyAlt)
        quickSwitch_ = false;

    sessionSaveTimer_ += io.DeltaTime;
    if (sessionSaveTimer_ > 30.f)
    {
        sessionSaveTimer_ = 0.f;
        saveSession();
    }

    core::PowerAction pa;
    if (kernel_.consumePowerRequest(pa))
    {
        if (pa == core::PowerAction::Shutdown || pa == core::PowerAction::Restart)
            glfwSetWindowShouldClose(window_, 1);
        else
            toast("Sleep requested");
    }
    kernel_.tick();

    desktopDim_ = anim::damp(desktopDim_, reagentOpen_ ? 1.f : 0.f, 12.f, ImGui::GetIO().DeltaTime);
    drawDesktop(size);
    {
        float tbReserve = Theme::get().taskbarH;
        if (!wm_.anyMaximized())
            tbReserve += kBarMargin * 2.f;
        wm_.drawAll(size, tbReserve);
    }

    /* Reap only when window fully closed (not minimized) */
    {
        std::vector<int> reap;
        for (const auto& p : kernel_.processes())
        {
            if (p.system || p.pid < 100) continue;
            auto* win = wm_.find(p.appId);
            if (!win)
                reap.push_back(p.pid);
        }
        for (int pid : reap)
            kernel_.exitProcess(pid);
    }

    drawTaskbar(size);
    drawReagent(size);
    drawContextMenu();
    drawPowerDialog(size);
    drawNotificationPanel(size);
    drawToasts(size);
    /* Hide embedded browser if Clor not open */
    {
        auto* cw = wm_.find("clor");
        bool show = cw && cw->open && cw->state != WState::Minimized;
        if (!show) velora::apps::EmbeddedBrowser::get().setVisible(false);
    }

    if (quickSwitch_)
    {
        ImGui::SetNextWindowPos(ImVec2(size.x * 0.5f, size.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(420, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 24.f);
        ImGui::Begin("##altswitch", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::TextUnformatted("Switch windows");
        ImGui::Separator();
        for (auto& w : wm_.list())
        {
            if (!w.open && w.state != WState::Minimized) continue;
            if (ImGui::Selectable(w.title.c_str(), w.focused))
            {
                wm_.restore(w.id);
                wm_.focus(w.id);
                quickSwitch_ = false;
            }
        }
        ImGui::End();
        ImGui::PopStyleVar();
    }

    /* Custom cursor overlay */
    Cursor::get().set(CursorKind::Pointer);
    if (ImGui::GetIO().WantTextInput)
        Cursor::get().set(CursorKind::Beam);
    Cursor::get().draw(window_);

    if (velora::system::SettingsStore::get().showFps)
    {
        char fps[32];
        std::snprintf(fps, sizeof(fps), "%.0f FPS", ImGui::GetIO().Framerate);
        ImGui::GetForegroundDrawList()->AddText(ImVec2(12, 12), IM_COL32(255, 255, 255, 180), fps);
    }

    ImGui::Render();
    glViewport(0, 0, fbW, fbH);
    glClearColor(0.02f, 0.03f, 0.04f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window_);
}

int Shell::run()
{
    while (window_ && !glfwWindowShouldClose(window_))
    {
        glfwPollEvents();
        frame();
    }
    return 0;
}
} // namespace velora::ui
