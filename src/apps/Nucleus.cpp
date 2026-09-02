#include "apps/Nucleus.hpp"
#include "core/Kernel.hpp"
#include "core/Access.hpp"
#include "fs/VelFS.hpp"
#include "system/Notifications.hpp"
#include "gfx/Graphics.hpp"
#include "system/PythonRuntime.hpp"

#include "imgui.h"
#include "ui/widgets/AppChrome.hpp"
#include "ui/widgets/Forms.hpp"
#include "ui/Theme.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <sstream>
#include <vector>

namespace {
const char* kCmds[] = {
    "help","clear","cls","echo","date","whoami","uname","neofetch","fetch",
    "pwd","cd","ls","dir","cat","head","touch","mkdir","rm","del","cp","mv",
    "ps","kill","uptime","free","df","env","history","notify","open","perm",
    "perms","log","dmesg","kernel","gfx","true","false","python","py","hostname","id","which",
    nullptr
};
void tryComplete(velora::apps::TermTab& t)
{
    std::string s = t.input;
    if (s.empty()) return;
    /* complete first word as command */
    if (s.find(' ') == std::string::npos)
    {
        std::vector<std::string> matches;
        for (int i = 0; kCmds[i]; ++i)
            if (std::strncmp(kCmds[i], s.c_str(), s.size()) == 0)
                matches.push_back(kCmds[i]);
        if (matches.size() == 1)
        {
            std::snprintf(t.input, sizeof(t.input), "%s", matches[0].c_str());
            return;
        }
        if (matches.size() > 1)
        {
            /* cycle shortest common prefix */
            std::string p = matches[0];
            for (auto& m : matches)
            {
                size_t n = 0;
                while (n < p.size() && n < m.size() && p[n] == m[n]) ++n;
                p = p.substr(0, n);
            }
            if (p.size() > s.size())
                std::snprintf(t.input, sizeof(t.input), "%s", p.c_str());
        }
        return;
    }
    /* complete path argument via VelFS */
    auto sp = s.find_last_of(' ');
    std::string prefix = s.substr(0, sp + 1);
    std::string partial = s.substr(sp + 1);
    std::string dir = t.cwd;
    std::string namePart = partial;
    auto slash = partial.find_last_of('/');
    if (slash != std::string::npos)
    {
        std::string sub = partial.substr(0, slash + 1);
        if (!sub.empty() && sub[0] == '/')
            dir = sub.substr(0, sub.size() - 1).empty() ? "/" : sub.substr(0, sub.size() - (sub.back()=='/'?1:0));
        else
            dir = velora::fs::VelFS::join(t.cwd, partial.substr(0, slash));
        namePart = partial.substr(slash + 1);
    }
    auto list = velora::fs::VelFS::get().list(dir.empty() ? t.cwd : dir);
    std::vector<std::string> matches;
    for (const auto& n : list)
        if (n.name.rfind(namePart, 0) == 0)
            matches.push_back(n.name + (n.isDir ? "/" : ""));
    if (matches.size() == 1)
    {
        std::string filled = prefix;
        if (slash != std::string::npos)
            filled += partial.substr(0, slash + 1) + matches[0];
        else
            filled += matches[0];
        std::snprintf(t.input, sizeof(t.input), "%s", filled.c_str());
    }
}

} // namespace

namespace velora::apps
{
Nucleus& Nucleus::get()
{
    static Nucleus n;
    return n;
}

TermTab& Nucleus::cur()
{
    if (tabs_.empty()) ensure();
    if (active_ < 0 || active_ >= (int)tabs_.size()) active_ = 0;
    return tabs_[active_];
}

void Nucleus::ensure()
{
    if (inited_) return;
    inited_ = true;
    tabs_.push_back({});
    auto& t = tabs_.back();
    println(t, "Nucleus privileged shell — VeloraOS", 5);
    println(t, "help · Tab complete · Ctrl+L clear · Up/Down history", 1);
    println(t, "", 0);
}

void Nucleus::println(TermTab& t, const std::string& s, int color)
{
    t.lines.push_back({s, color});
    if (t.lines.size() > 2000)
        t.lines.erase(t.lines.begin(), t.lines.begin() + 500);
}

std::vector<std::string> Nucleus::splitArgs(const std::string& s)
{
    std::vector<std::string> out;
    std::string cur;
    bool q = false;
    for (char c : s)
    {
        if (c == '"') { q = !q; continue; }
        if (!q && (c == ' ' || c == '\t'))
        {
            if (!cur.empty()) { out.push_back(cur); cur.clear(); }
            continue;
        }
        cur.push_back(c);
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

std::string Nucleus::resolve(TermTab& t, const std::string& path)
{
    if (path.empty()) return t.cwd;
    if (path[0] == '/') return path;
    return velora::fs::VelFS::join(t.cwd, path);
}

void Nucleus::exec(TermTab& t, velora::core::Kernel& k, const std::string& line)
{
    std::string s = line;
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(s.begin());
    if (s.empty()) return;

    t.history.push_back(s);
    t.histIdx = -1;

    println(t, "nucleus@velora:" + t.cwd + "$ " + s, 1);

    /* simple && chain */
    std::vector<std::string> parts;
    {
        std::string cur;
        for (size_t i = 0; i < s.size(); ++i)
        {
            if (i + 1 < s.size() && s[i] == '&' && s[i + 1] == '&')
            {
                if (!cur.empty()) parts.push_back(cur);
                cur.clear();
                ++i;
                continue;
            }
            cur.push_back(s[i]);
        }
        if (!cur.empty()) parts.push_back(cur);
    }
    for (auto& p : parts)
    {
        while (!p.empty() && p.front() == ' ') p.erase(p.begin());
        execOne(t, k, p);
    }
}

void Nucleus::execOne(TermTab& t, velora::core::Kernel& k, const std::string& cmd)
{
    auto args = splitArgs(cmd);
    if (args.empty()) return;
    const std::string& c = args[0];
    auto& vfs = velora::fs::VelFS::get();

    if (c == "help" || c == "?")
    {
        println(t, "Built-in commands:", 5);
        println(t, "  help, clear, echo, date, whoami, uname, neofetch", 0);
        println(t, "  pwd, cd, ls, cat, head, touch, mkdir, rm, cp, mv", 0);
        println(t, "  ps, kill <pid>, uptime, free, df, env", 0);
        println(t, "  history, notify <text>, open <app>, perm <app>", 0);
        println(t, "  log, kernel, gfx, python, true, false", 0);
        println(t, "Chain with &&   Quotes: echo \"hello world\"", 1);
        return;
    }
    if (c == "clear" || c == "cls")
    {
        t.lines.clear();
        return;
    }
    if (c == "echo")
    {
        std::string out;
        for (size_t i = 1; i < args.size(); ++i)
        {
            if (i > 1) out += ' ';
            out += args[i];
        }
        println(t, out, 0);
        return;
    }
    if (c == "date")
    {
        std::time_t now = std::time(nullptr);
        std::tm tm{};
#if defined(_MSC_VER)
        localtime_s(&tm, &now);
#else
        if (auto* p = std::localtime(&now)) tm = *p;
#endif
        char buf[64];
        std::strftime(buf, sizeof(buf), "%a %Y-%m-%d %H:%M:%S", &tm);
        println(t, buf, 0);
        return;
    }
    if (c == "whoami")
    {
        println(t, "nucleus", 0);
        return;
    }
    if (c == "uname")
    {
        println(t, "VeloraOS " + k.version() + " Clorium/" + k.name(), 0);
        return;
    }
    if (c == "neofetch" || c == "fetch")
    {
        const auto& gi = velora::gfx::Graphics::get().info();
        println(t, "        ████  nucleus@velora", 5);
        println(t, "       ██  ██ OS: VeloraOS " + k.version(), 0);
        println(t, "       ██  ██ Kernel: " + k.name(), 0);
        println(t, "        ████  Uptime: " + std::to_string(k.uptimeSec()) + "s", 0);
        println(t, "              Shell: Nucleus", 0);
        println(t, "              CPU: " + std::to_string((int)k.cpuUsage()) + "%", 0);
        println(t, "              Mem: " + std::to_string((int)k.memUsedMb()) + "/" +
                         std::to_string((int)k.memTotalMb()) + " MB", 0);
        println(t, "              GPU: " + gi.renderer, 0);
        println(t, "              API: " + std::string(velora::gfx::Graphics::get().backendName()), 0);
        return;
    }
    if (c == "pwd")
    {
        println(t, t.cwd, 0);
        return;
    }
    if (c == "cd")
    {
        std::string dest = args.size() > 1 ? resolve(t, args[1]) : "/User";
        if (args.size() > 1 && args[1] == "~") dest = "/User";
        if (args.size() > 1 && args[1] == "..") dest = velora::fs::VelFS::parent(t.cwd);
        if (!vfs.isDir(dest) && dest != "/")
        {
            println(t, "cd: no such directory: " + dest, 4);
            return;
        }
        t.cwd = dest;
        return;
    }
    if (c == "ls" || c == "dir")
    {
        std::string path = args.size() > 1 ? resolve(t, args[1]) : t.cwd;
        auto list = vfs.list(path);
        if (list.empty() && !vfs.isDir(path))
        {
            println(t, "ls: cannot access " + path, 4);
            return;
        }
        for (const auto& n : list)
        {
            char line[256];
            if (n.isDir)
                std::snprintf(line, sizeof(line), "drwx  %8s  %s", "-", n.name.c_str());
            else
                std::snprintf(line, sizeof(line), "-rw-  %8zu  %s", n.size, n.name.c_str());
            println(t, line, n.isDir ? 5 : 0);
        }
        println(t, std::to_string(list.size()) + " items", 1);
        return;
    }
    if (c == "cat")
    {
        if (args.size() < 2) { println(t, "usage: cat <file>", 3); return; }
        std::string path = resolve(t, args[1]);
        if (!vfs.exists(path) || vfs.isDir(path))
        {
            println(t, "cat: " + path + ": not a file", 4);
            return;
        }
        auto text = vfs.readText(path);
        if (text.size() > 16000) text = text.substr(0, 16000) + "\n…";
        std::istringstream in(text);
        std::string line;
        while (std::getline(in, line))
            println(t, line, 0);
        return;
    }
    if (c == "head")
    {
        if (args.size() < 2) { println(t, "usage: head <file>", 3); return; }
        auto text = vfs.readText(resolve(t, args[1]));
        std::istringstream in(text);
        std::string line;
        int n = 0;
        while (std::getline(in, line) && n++ < 10)
            println(t, line, 0);
        return;
    }
    if (c == "touch")
    {
        if (args.size() < 2) { println(t, "usage: touch <file>", 3); return; }
        std::string path = resolve(t, args[1]);
        if (!vfs.exists(path))
            vfs.writeText(path, "");
        println(t, "touched " + path, 2);
        return;
    }
    if (c == "mkdir")
    {
        if (args.size() < 2) { println(t, "usage: mkdir <dir>", 3); return; }
        std::string path = resolve(t, args[1]);
        if (vfs.mkdir(path))
            println(t, "created " + path, 2);
        else
            println(t, "mkdir failed", 4);
        return;
    }
    if (c == "rm" || c == "del")
    {
        if (args.size() < 2) { println(t, "usage: rm <path>", 3); return; }
        std::string path = resolve(t, args[1]);
        if (vfs.remove(path))
            println(t, "removed " + path, 2);
        else
            println(t, "rm: failed " + path, 4);
        return;
    }
    if (c == "cp")
    {
        if (args.size() < 3) { println(t, "usage: cp <src> <dst>", 3); return; }
        if (vfs.copy(resolve(t, args[1]), resolve(t, args[2])))
            println(t, "copied", 2);
        else
            println(t, "cp failed", 4);
        return;
    }
    if (c == "mv")
    {
        if (args.size() < 3) { println(t, "usage: mv <src> <dst>", 3); return; }
        if (vfs.move(resolve(t, args[1]), resolve(t, args[2])))
            println(t, "moved", 2);
        else
            println(t, "mv failed", 4);
        return;
    }
    if (c == "ps")
    {
        println(t, "  PID  PPID  STATE     MEM   NAME", 1);
        for (const auto& p : k.processes())
        {
            char line[128];
            std::snprintf(line, sizeof(line), "%5d %5d  %-8s %5.0f  %s",
                          p.pid, p.ppid, p.state.c_str(), p.memMb, p.name.c_str());
            println(t, line, p.system ? 1 : 0);
        }
        return;
    }
    if (c == "kill")
    {
        if (args.size() < 2) { println(t, "usage: kill <pid>", 3); return; }
        int pid = std::atoi(args[1].c_str());
        if (k.killProcess(pid))
            println(t, "killed " + std::to_string(pid), 2);
        else
            println(t, "kill: failed", 4);
        return;
    }
    if (c == "uptime")
    {
        long long s = k.uptimeSec();
        println(t, "up " + std::to_string(s / 3600) + "h " +
                       std::to_string((s % 3600) / 60) + "m " + std::to_string(s % 60) + "s",
                0);
        return;
    }
    if (c == "free")
    {
        println(t, "Mem: " + std::to_string((int)k.memUsedMb()) + " / " +
                       std::to_string((int)k.memTotalMb()) + " MB  CPU: " +
                       std::to_string((int)k.cpuUsage()) + "%",
                0);
        return;
    }
    if (c == "df")
    {
        println(t, "Filesystem  Mount", 1);
        println(t, "VelFS       " + vfs.hostRoot(), 0);
        println(t, "  /User  /System  /Applications", 1);
        return;
    }
    if (c == "env")
    {
        println(t, "SHELL=nucleus", 0);
        println(t, "HOME=/User", 0);
        println(t, "PWD=" + t.cwd, 0);
        println(t, "OS=VeloraOS", 0);
        println(t, "USER=nucleus", 0);
        return;
    }
    if (c == "history")
    {
        for (size_t i = 0; i < t.history.size(); ++i)
            println(t, std::to_string(i + 1) + "  " + t.history[i], 0);
        return;
    }
    if (c == "notify")
    {
        std::string msg;
        for (size_t i = 1; i < args.size(); ++i)
        {
            if (i > 1) msg += ' ';
            msg += args[i];
        }
        if (msg.empty()) msg = "ping from nucleus";
        velora::system::NotificationCenter::get().post("Nucleus", msg, "terminal");
        println(t, "notification sent", 2);
        return;
    }
    if (c == "open")
    {
        if (args.size() < 2) { println(t, "usage: open <appId>", 3); return; }
        println(t, "request open " + args[1] + " (use Reagent/desktop)", 3);
        return;
    }
    if (c == "perm" || c == "perms")
    {
        std::string app = args.size() > 1 ? args[1] : "terminal";
        auto m = velora::core::AccessControl::get().mask(app);
        println(t, "permissions for " + app + ":", 5);
        auto show = [&](velora::core::Perm bit, const char* n) {
            println(t, std::string("  ") + n + ": " + (velora::core::hasPerm(m, bit) ? "yes" : "no"),
                    velora::core::hasPerm(m, bit) ? 2 : 1);
        };
        show(velora::core::Perm::FilesRead, "files.read");
        show(velora::core::Perm::FilesWrite, "files.write");
        show(velora::core::Perm::Network, "network");
        show(velora::core::Perm::Notifications, "notifications");
        show(velora::core::Perm::ProcessKill, "process.kill");
        show(velora::core::Perm::System, "system");
        return;
    }
    if (c == "log" || c == "dmesg")
    {
        for (const auto& e : k.events())
            println(t, std::string("[") + e.time + "] " + e.message, 1);
        return;
    }
    if (c == "kernel")
    {
        println(t, k.name() + " " + k.version() + " state=" + k.state(), 0);
        return;
    }
    if (c == "gfx")
    {
        const auto& gi = velora::gfx::Graphics::get().info();
        println(t, std::string(velora::gfx::Graphics::get().backendName()) + " · " + gi.renderer, 0);
        println(t, gi.version, 1);
        return;
    }
    if (c == "python" || c == "py")
    {
        auto& py = velora::system::PythonRuntime::get();
        if (!py.detect())
        {
            println(t, "python: interpreter not found", 4);
            return;
        }
        if (args.size() == 1)
        {
            println(t, py.interpreter(), 2);
            println(t, "usage: python <file.py> | python -c \"code\"", 1);
            return;
        }
        if (args[1] == "-c" && args.size() >= 3)
        {
            std::string code;
            for (size_t i = 2; i < args.size(); ++i)
            {
                if (i > 2) code += ' ';
                code += args[i];
            }
            auto res = py.runCode(code, t.cwd);
            if (!res.output.empty())
            {
                std::istringstream in(res.output);
                std::string line;
                while (std::getline(in, line))
                    println(t, line, res.ok ? 0 : 4);
            }
            if (!res.ok) println(t, "exit " + std::to_string(res.exitCode), 4);
            return;
        }
        std::string script = resolve(t, args[1]);
        /* map virtual to host if under VelFS */
        std::string hostScript = script;
        if (!script.empty() && script[0] == '/')
            hostScript = velora::fs::VelFS::get().hostPath(script);
        std::string hostCwd = t.cwd;
        if (!hostCwd.empty() && hostCwd[0] == '/')
            hostCwd = velora::fs::VelFS::get().hostPath(t.cwd);
        auto res = py.runFile(hostScript, hostCwd);
        if (!res.output.empty())
        {
            std::istringstream in(res.output);
            std::string line;
            while (std::getline(in, line))
                println(t, line, res.ok ? 0 : 4);
        }
        else if (!res.error.empty())
            println(t, res.error, 4);
        if (!res.ok) println(t, "exit " + std::to_string(res.exitCode), 4);
        return;
    }
    if (c == "hostname") { println(t, "velora", 0); return; }
    if (c == "id") { println(t, "uid=0(nucleus) gid=0(clorium)", 0); return; }
    if (c == "which")
    {
        if (args.size() < 2) { println(t, "usage: which <cmd>", 3); return; }
        bool found = false;
        for (int i = 0; kCmds[i]; ++i)
            if (args[1] == kCmds[i]) { println(t, std::string("/bin/") + kCmds[i], 2); found = true; break; }
        if (!found) println(t, args[1] + " not found", 4);
        return;
    }
    if (c == "true") { return; }
    if (c == "false") { println(t, "false", 4); return; }

    println(t, c + ": command not found", 4);
    println(t, "Try 'help'", 1);
}


void Nucleus::draw(velora::core::Kernel& kernel)
{
    ensure();
    auto& th = velora::ui::Theme::get();
    using namespace velora::ui::forms;

    /* Header */
    ImVec2 hp = ImGui::GetCursorScreenPos();
    float hw = ImGui::GetContentRegionAvail().x;
    ImGui::GetWindowDrawList()->AddRectFilled(hp, ImVec2(hp.x + hw, hp.y + 48),
        IM_COL32(20, 20, 24, 255));
    ImGui::GetWindowDrawList()->AddRectFilled(hp, ImVec2(hp.x + 4, hp.y + 48),
        ImGui::ColorConvertFloat4ToU32(th.accent));
    ImGui::SetCursorScreenPos(ImVec2(hp.x + 16, hp.y + 12));
    ImGui::TextUnformatted("Nucleus");
    ImGui::SameLine();
    ImGui::TextDisabled(" · privileged shell");
    ImGui::SetCursorScreenPos(ImVec2(hp.x, hp.y + 52));

    /* tabs */
    for (int i = 0; i < (int)tabs_.size(); ++i)
    {
        if (i) ImGui::SameLine(0, 4);
        ImGui::PushID(i);
        bool on = (i == active_);
        if (on) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(th.accent.x, th.accent.y, th.accent.z, 0.35f));
        if (ImGui::Button(tabs_[i].title.c_str(), ImVec2(0, 28)))
            active_ = i;
        if (on) ImGui::PopStyleColor();
        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Close") && tabs_.size() > 1)
            {
                tabs_.erase(tabs_.begin() + i);
                if (active_ >= (int)tabs_.size()) active_ = (int)tabs_.size() - 1;
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
    ImGui::SameLine(0, 6);
    if (TonalButton("+", ImVec2(28, 28)))
    {
        tabs_.push_back({});
        tabs_.back().title = "sh-" + std::to_string(tabs_.size());
        println(tabs_.back(), "New Nucleus session", 5);
        active_ = (int)tabs_.size() - 1;
    }
    ImGui::SameLine();
    if (OutlinedButton("Clear", ImVec2(56, 28)))
        cur().lines.clear();

    auto& t = cur();

    /* Terminal body — dark surface */
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.06f, 0.07f, 0.09f, 1.f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 16.f);
    ImGui::BeginChild("##termout", ImVec2(0, -44), true);
    for (const auto& line : t.lines)
    {
        ImVec4 col(0.86f, 0.88f, 0.90f, 1);
        if (line.color == 1) col = ImVec4(0.50f, 0.52f, 0.56f, 1);
        if (line.color == 2) col = ImVec4(0.40f, 0.90f, 0.55f, 1);
        if (line.color == 3) col = ImVec4(0.95f, 0.78f, 0.30f, 1);
        if (line.color == 4) col = ImVec4(0.95f, 0.42f, 0.42f, 1);
        if (line.color == 5) col = ImVec4(th.accent.x, th.accent.y, th.accent.z, 1);
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        ImGui::TextUnformatted(line.text.c_str());
        ImGui::PopStyleColor();
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 8.f)
        ImGui::SetScrollHereY(1.f);
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    /* Input bar */
    ImVec2 ip = ImGui::GetCursorScreenPos();
    float iw = ImGui::GetContentRegionAvail().x;
    ImGui::GetWindowDrawList()->AddRectFilled(ip, ImVec2(ip.x + iw, ip.y + 40),
        IM_COL32(18, 18, 22, 255), 12.f);
    ImGui::SetCursorScreenPos(ImVec2(ip.x + 12, ip.y + 8));
    std::string prompt = "nucleus:" + t.cwd + " $";
    ImGui::TextColored(th.accent, "%s", prompt.c_str());
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);

    ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue |
                                ImGuiInputTextFlags_CallbackHistory |
                                ImGuiInputTextFlags_CallbackCompletion;
    auto histCb = [](ImGuiInputTextCallbackData* data) -> int {
        auto* tab = (TermTab*)data->UserData;
        if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion)
        {
            std::snprintf(tab->input, sizeof(tab->input), "%s", data->Buf);
            tryComplete(*tab);
            data->DeleteChars(0, data->BufTextLen);
            data->InsertChars(0, tab->input);
            return 0;
        }
        if (data->EventFlag != ImGuiInputTextFlags_CallbackHistory) return 0;
        if (tab->history.empty()) return 0;
        if (data->EventKey == ImGuiKey_UpArrow)
        {
            if (tab->histIdx < 0) tab->histIdx = (int)tab->history.size() - 1;
            else if (tab->histIdx > 0) tab->histIdx--;
        }
        else if (data->EventKey == ImGuiKey_DownArrow)
        {
            if (tab->histIdx >= 0) tab->histIdx++;
            if (tab->histIdx >= (int)tab->history.size())
            {
                tab->histIdx = -1;
                data->DeleteChars(0, data->BufTextLen);
                return 0;
            }
        }
        if (tab->histIdx >= 0 && tab->histIdx < (int)tab->history.size())
        {
            data->DeleteChars(0, data->BufTextLen);
            data->InsertChars(0, tab->history[tab->histIdx].c_str());
        }
        return 0;
    };

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_L))
        t.lines.clear();

    if (ImGui::IsWindowAppearing())
        ImGui::SetKeyboardFocusHere();
    bool submit = ImGui::InputText("##ncmd", t.input, sizeof(t.input), flags, histCb, &t);
    if (submit)
    {
        std::string line = t.input;
        t.input[0] = 0;
        exec(t, kernel, line);
        ImGui::SetKeyboardFocusHere(-1);
    }
}


} // namespace velora::apps
