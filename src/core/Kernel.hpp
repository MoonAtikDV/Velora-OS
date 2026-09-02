#pragma once

#include "core/Access.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace velora::core
{
struct LogEvent { std::string time; std::string message; };

enum class PowerAction { Sleep, Restart, Shutdown, Lock };

class Kernel
{
public:
    bool initialize();
    void shutdown();
    void tick();

    const std::string& name() const { return name_; }
    const std::string& version() const { return version_; }
    const std::string& state() const { return state_; }
    long long uptimeSec() const;

    void log(const std::string& message);
    const std::vector<LogEvent>& events() const { return events_; }

    const std::vector<ProcessInfo>& processes() const { return processes_; }
    ProcessInfo* findProcess(int pid);
    const ProcessInfo* findProcess(int pid) const;
    ProcessInfo* findByApp(const std::string& appId);

    /** Spawn process for an app window. Returns PID. */
    int spawn(const std::string& appId, const std::string& name, bool systemPkg = false);
    bool exitProcess(int pid);
    bool killProcess(int pid); /* requires ProcessKill on caller; system protected */

    void setCurrentPid(int pid) { currentPid_ = pid; }
    int currentPid() const { return currentPid_; }

    bool check(Perm need) const;
    bool checkApp(const std::string& appId, Perm need) const;

    void requestPower(PowerAction a);
    bool consumePowerRequest(PowerAction& out);

    float cpuUsage() const { return cpuUsage_; }
    float memUsedMb() const { return memUsed_; }
    float memTotalMb() const { return memTotal_; }

    void refreshProcesses(); /* rebuild system services only; keeps app PIDs */

private:
    std::string name_ = "Clorium";
    std::string version_ = "0.7.0";
    std::string state_ = "booting";
    std::chrono::steady_clock::time_point started_{};
    std::vector<LogEvent> events_;
    std::vector<ProcessInfo> processes_;
    PowerAction powerReq_ = PowerAction::Lock;
    bool powerPending_ = false;
    float cpuUsage_ = 0.f;
    float memUsed_ = 512.f;
    float memTotal_ = 8192.f;
    int nextPid_ = 100; /* user/app PIDs from 100+; system 1-99 */
    int currentPid_ = 1;
    int pidClorium_ = 1;
    int pidShell_ = 2;
};
} // namespace velora::core
