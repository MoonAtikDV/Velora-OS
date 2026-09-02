#pragma once

#include "ui/Icons.hpp"

#include <string>
#include <vector>

namespace velora::ui
{
struct AppInfo
{
    std::string id;
    std::string name;
    const char* icon; /* Material Symbols UTF-8 */
    bool system = true;
};

inline const std::vector<AppInfo>& defaultCatalog()
{
    static const std::vector<AppInfo> apps = {
        {"settings", "Settings", icons::settings(), true},
        {"system", "System", icons::info(), true},
        {"files", "Lattice", icons::folder(), true},
        {"terminal", "Nucleus", icons::terminal(), true},
        {"calculator", "Calculator", icons::calculate(), true},
        {"clor", "Clor", icons::language(), true},
        {"clock", "Clock", icons::schedule(), true},
        {"notes", "Notes", icons::note(), true},
        {"gallery", "Gallery", icons::image(), true},
        {"logs", "Logs", icons::description(), true},
    };
    return apps;
}

inline const AppInfo* findApp(const std::string& id)
{
    for (const auto& a : defaultCatalog())
        if (a.id == id) return &a;
    return nullptr;
}
} // namespace velora::ui
