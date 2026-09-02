#pragma once
#include <string>
#include <vector>

namespace velora::system
{
struct DeskEntry { std::string id; int col = 1; int row = 1; };

struct SessionData
{
    int wallpaper = 1;
    float taskbarH = 64.f;
    float rounding = 20.f;
    float taskbarOpacity = 0.38f;
    bool animations = true;
    bool blurDock = true;
    bool accentFromWallpaper = true;
    bool vsync = true;
    bool showFps = false;
    bool notifEnabled = true;
    float uiScale = 1.f;
    int language = 0;
    std::vector<std::string> pins;
    std::vector<DeskEntry> desktop;
};

class Session
{
public:
    static Session& get();
    bool load(SessionData& out);
    bool save(const SessionData& in);
    std::string path() const;
};
} // namespace velora::system
