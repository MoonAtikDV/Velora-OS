#include "core/Kernel.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <ctime>

namespace velora::core
{
bool Kernel::initialize()
{
    started_ = std::chrono::steady_clock::now();
    state_ = "running";
    processes_.clear();
    nextPid_ = 100;

    auto addSys = [&](int pid, const char* name, float cpu, float mem) {
        ProcessInfo p;
        p.pid = pid;
        p.ppid = 0;
        p.name = name;
        p.appId = name;
        p.state = "running";
        p.cpu = cpu;
        p.memMb = mem;
        p.system = true;
        p.perms = Perm::All;
        processes_.push_back(p);
        AccessControl::get().setMask(name, Perm::All);
    };
    pidClorium_ = 1;
    pidShell_ = 2;
    addSys(1, "clorium", 2.f, 48.f);
    addSys(2, "shell", 4.f, 96.f);
    addSys(3, "velfs", 1.f, 24.f);
    addSys(4, "icon-cache", 0.5f, 16.f);
    currentPid_ = 1;

    log("Clorium kernel initialized");
    log("Access control online");
    log("PID allocator ready (app PIDs >= 100)");
    return true;
}

void Kernel::shutdown()
{
    state_ = "stopped";
    log("Kernel shutdown");
}

void Kernel::tick()
{
    static float t = 0.f;
    t += 0.016f;
    cpuUsage_ = 8.f + 12.f * (0.5f + 0.5f * std::sin(t * 0.7f));
    memUsed_ = 480.f + 40.f * (0.5f + 0.5f * std::sin(t * 0.3f));
    for (auto& p : processes_)
    {
        if (p.state == "running")
            p.cpu = std::max(0.05f, p.cpu * 0.9f + (float)(rand() % 15) * 0.04f);
    }
}

long long Kernel::uptimeSec() const
{
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::steady_clock::now() - started_)
        .count();
}

void Kernel::log(const std::string& message)
{
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#if defined(_MSC_VER)
    localtime_s(&tm, &t);
#else
    if (auto* p = std::localtime(&t)) tm = *p;
#endif
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
    events_.push_back({buf, message});
    if (events_.size() > 500) events_.erase(events_.begin(), events_.begin() + 100);
}

ProcessInfo* Kernel::findProcess(int pid)
{
    for (auto& p : processes_)
        if (p.pid == pid) return &p;
    return nullptr;
}

const ProcessInfo* Kernel::findProcess(int pid) const
{
    for (const auto& p : processes_)
        if (p.pid == pid) return &p;
    return nullptr;
}

ProcessInfo* Kernel::findByApp(const std::string& appId)
{
    for (auto& p : processes_)
        if (p.appId == appId && p.state == "running") return &p;
    return nullptr;
}

int Kernel::spawn(const std::string& appId, const std::string& name, bool systemPkg)
{
    if (auto* existing = findByApp(appId))
    {
        existing->state = "running";
        currentPid_ = existing->pid;
        return existing->pid;
    }

    ProcessInfo p;
    p.pid = nextPid_++;
    p.ppid = pidShell_;
    p.name = name;
    p.appId = appId;
    p.state = "running";
    p.cpu = 1.f;
    p.memMb = 32.f + (float)(rand() % 40);
    p.system = systemPkg;
    p.perms = AccessControl::get().mask(appId);
    if (p.perms == Perm::None || static_cast<std::uint32_t>(p.perms) == 0)
        p.perms = defaultPermsFor(appId, systemPkg);
    AccessControl::get().setMask(appId, p.perms);
    processes_.push_back(p);
    currentPid_ = p.pid;
    log("spawn pid=" + std::to_string(p.pid) + " app=" + appId);
    return p.pid;
}

bool Kernel::exitProcess(int pid)
{
    auto* p = findProcess(pid);
    if (!p || p->system) return false;
    log("exit pid=" + std::to_string(pid) + " app=" + p->appId);
    processes_.erase(std::remove_if(processes_.begin(), processes_.end(),
                                    [&](const ProcessInfo& x) { return x.pid == pid; }),
                     processes_.end());
    if (currentPid_ == pid) currentPid_ = pidShell_;
    return true;
}

bool Kernel::killProcess(int pid)
{
    auto* p = findProcess(pid);
    if (!p) return false;
    if (p->system)
    {
        log("Refused kill system pid=" + std::to_string(pid));
        return false;
    }
    log("kill pid=" + std::to_string(pid) + " " + p->name);
    return exitProcess(pid);
}

bool Kernel::check(Perm need) const
{
    const ProcessInfo* p = findProcess(currentPid_);
    if (!p) return false;
    return hasPerm(p->perms, need);
}

bool Kernel::checkApp(const std::string& appId, Perm need) const
{
    return AccessControl::get().check(appId, need);
}

void Kernel::requestPower(PowerAction a)
{
    powerReq_ = a;
    powerPending_ = true;
    const char* n = "lock";
    if (a == PowerAction::Sleep) n = "sleep";
    if (a == PowerAction::Restart) n = "restart";
    if (a == PowerAction::Shutdown) n = "shutdown";
    log(std::string("Power request: ") + n);
}

bool Kernel::consumePowerRequest(PowerAction& out)
{
    if (!powerPending_) return false;
    out = powerReq_;
    powerPending_ = false;
    return true;
}

void Kernel::refreshProcesses()
{
    /* keep app processes; ensure system services exist */
    auto ensure = [&](int pid, const char* name) {
        if (!findProcess(pid))
        {
            ProcessInfo p;
            p.pid = pid;
            p.ppid = 0;
            p.name = name;
            p.appId = name;
            p.state = "running";
            p.system = true;
            p.perms = Perm::All;
            processes_.push_back(p);
        }
    };
    ensure(1, "clorium");
    ensure(2, "shell");
    ensure(3, "velfs");
    ensure(4, "icon-cache");
}
} // namespace velora::core
