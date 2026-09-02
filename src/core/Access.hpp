#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace velora::core
{
enum class Perm : std::uint32_t
{
    None          = 0,
    FilesRead     = 1u << 0,
    FilesWrite    = 1u << 1,
    Network       = 1u << 2,
    Notifications = 1u << 3,
    ProcessList   = 1u << 4,
    ProcessKill   = 1u << 5,
    Power         = 1u << 6,
    Clipboard     = 1u << 7,
    Settings      = 1u << 8,
    System        = 1u << 9,  /* kernel-level */
    All           = 0xFFFFFFFFu
};

inline Perm operator|(Perm a, Perm b)
{
    return static_cast<Perm>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}
inline Perm operator&(Perm a, Perm b)
{
    return static_cast<Perm>(static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b));
}
inline bool hasPerm(Perm set, Perm need)
{
    return (static_cast<std::uint32_t>(set) & static_cast<std::uint32_t>(need)) ==
           static_cast<std::uint32_t>(need);
}

inline const char* permName(Perm p)
{
    switch (p)
    {
    case Perm::FilesRead: return "files.read";
    case Perm::FilesWrite: return "files.write";
    case Perm::Network: return "network";
    case Perm::Notifications: return "notifications";
    case Perm::ProcessList: return "process.list";
    case Perm::ProcessKill: return "process.kill";
    case Perm::Power: return "power";
    case Perm::Clipboard: return "clipboard";
    case Perm::Settings: return "settings";
    case Perm::System: return "system";
    default: return "none";
    }
}

struct ProcessInfo
{
    int pid = 0;
    int ppid = 1; /* clorium */
    std::string name;
    std::string appId; /* package id if any */
    std::string state; /* running | sleeping | suspended | zombie */
    float cpu = 0.f;
    float memMb = 0.f;
    bool system = false;
    Perm perms = Perm::None;
};

/** Default permission profile for known packages */
inline Perm defaultPermsFor(const std::string& appId, bool systemPkg)
{
    if (systemPkg || appId == "system" || appId == "taskmgr" || appId == "terminal" || appId == "logs")
        return Perm::All;
    if (appId == "settings")
        return Perm::Settings | Perm::FilesRead | Perm::Notifications | Perm::Power | Perm::ProcessList;
    if (appId == "files")
        return Perm::FilesRead | Perm::FilesWrite | Perm::Notifications | Perm::Clipboard;
    if (appId == "clor")
        return Perm::Network | Perm::Notifications | Perm::Clipboard;
    if (appId == "notes")
        return Perm::FilesRead | Perm::FilesWrite | Perm::Notifications;
    if (appId == "gallery")
        return Perm::FilesRead;
    /* generic user app */
    return Perm::Notifications | Perm::FilesRead;
}

class AccessControl
{
public:
    static AccessControl& get();

    void grant(const std::string& appId, Perm p);
    void revoke(const std::string& appId, Perm p);
    void setMask(const std::string& appId, Perm mask);
    Perm mask(const std::string& appId) const;

    bool check(const std::string& appId, Perm need) const;
    bool checkPid(int pid, Perm need) const; /* looks up process table via Kernel */

    std::vector<std::string> listKnownApps() const;

private:
    std::unordered_map<std::string, Perm> grants_;
};
} // namespace velora::core
