#include "system/Session.hpp"
#include "fs/VelFS.hpp"
#include <cstdlib>
#include <iostream>
#include <sstream>

namespace velora::system
{
Session& Session::get() { static Session s; return s; }

std::string Session::path() const { return "/System/session.cfg"; }

bool Session::load(SessionData& out)
{
    auto raw = velora::fs::VelFS::get().readText(path());
    if (raw.empty()) return false;
    out = SessionData{};
    std::istringstream in(raw);
    std::string line;
    while (std::getline(in, line))
    {
        if (line.empty() || line[0] == '#') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq);
        std::string v = line.substr(eq + 1);
        auto b = [&](const char* key) { return k == key && (v == "1" || v == "true"); };
        if (k == "wallpaper") out.wallpaper = std::atoi(v.c_str());
        else if (k == "taskbarH") out.taskbarH = (float)std::atof(v.c_str());
        else if (k == "rounding") out.rounding = (float)std::atof(v.c_str());
        else if (k == "taskbarOpacity") out.taskbarOpacity = (float)std::atof(v.c_str());
        else if (k == "uiScale") out.uiScale = (float)std::atof(v.c_str());
        else if (k == "language") out.language = std::atoi(v.c_str());
        else if (k == "animations") out.animations = (v == "1" || v == "true");
        else if (k == "blurDock") out.blurDock = (v == "1" || v == "true");
        else if (k == "accentFromWallpaper") out.accentFromWallpaper = (v == "1" || v == "true");
        else if (k == "vsync") out.vsync = (v == "1" || v == "true");
        else if (k == "showFps") out.showFps = (v == "1" || v == "true");
        else if (k == "notifEnabled") out.notifEnabled = (v == "1" || v == "true");
        else if (k == "pin") out.pins.push_back(v);
        else if (k == "desk")
        {
            DeskEntry e;
            auto c1 = v.find(',');
            auto c2 = v.find(',', c1 == std::string::npos ? 0 : c1 + 1);
            if (c1 == std::string::npos) continue;
            e.id = v.substr(0, c1);
            e.col = std::atoi(v.substr(c1 + 1, c2 == std::string::npos ? std::string::npos : c2 - c1 - 1).c_str());
            e.row = c2 == std::string::npos ? 1 : std::atoi(v.substr(c2 + 1).c_str());
            out.desktop.push_back(e);
        }
    }
    std::cout << "[session] loaded desk=" << out.desktop.size() << " pins=" << out.pins.size() << std::endl;
    return true;
}

bool Session::save(const SessionData& in)
{
    std::ostringstream o;
    o << "# VeloraOS session\n";
    o << "wallpaper=" << in.wallpaper << "\n";
    o << "taskbarH=" << in.taskbarH << "\n";
    o << "rounding=" << in.rounding << "\n";
    o << "taskbarOpacity=" << in.taskbarOpacity << "\n";
    o << "uiScale=" << in.uiScale << "\n";
    o << "language=" << in.language << "\n";
    o << "animations=" << (in.animations ? 1 : 0) << "\n";
    o << "blurDock=" << (in.blurDock ? 1 : 0) << "\n";
    o << "accentFromWallpaper=" << (in.accentFromWallpaper ? 1 : 0) << "\n";
    o << "vsync=" << (in.vsync ? 1 : 0) << "\n";
    o << "showFps=" << (in.showFps ? 1 : 0) << "\n";
    o << "notifEnabled=" << (in.notifEnabled ? 1 : 0) << "\n";
    for (const auto& p : in.pins) o << "pin=" << p << "\n";
    for (const auto& d : in.desktop)
        o << "desk=" << d.id << "," << d.col << "," << d.row << "\n";
    bool ok = velora::fs::VelFS::get().writeText(path(), o.str());
    if (ok) std::cout << "[session] saved\n";
    return ok;
}
} // namespace velora::system
