#include "system/PythonRuntime.hpp"
#include "fs/VelFS.hpp"

#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace stdfs = std::filesystem;

namespace velora::system
{
PythonRuntime& PythonRuntime::get()
{
    static PythonRuntime r;
    return r;
}

bool PythonRuntime::detect()
{
    if (detected_) return available();
    detected_ = true;

    const char* cands[] = {
#ifdef _WIN32
        "python", "python3", "py",
#else
        "python3", "python",
#endif
    };

    for (const char* c : cands)
    {
#ifdef _WIN32
        /* where.exe */
        std::string cmd = std::string("where ") + c + " 2>NUL";
        FILE* pipe = _popen(cmd.c_str(), "r");
#else
        std::string cmd = std::string("command -v ") + c + " 2>/dev/null";
        FILE* pipe = popen(cmd.c_str(), "r");
#endif
        if (!pipe) continue;
        char buf[512]{};
        if (std::fgets(buf, sizeof(buf), pipe))
        {
            std::string path = buf;
            while (!path.empty() && (path.back() == '\n' || path.back() == '\r'))
                path.pop_back();
            if (!path.empty())
            {
                interp_ = path;
#ifdef _WIN32
                /* on Windows `py` needs -3 sometimes; prefer full path from where */
#endif
                std::cout << "[python] interpreter: " << interp_ << std::endl;
#ifdef _WIN32
                _pclose(pipe);
#else
                pclose(pipe);
#endif
                return true;
            }
        }
#ifdef _WIN32
        _pclose(pipe);
#else
        pclose(pipe);
#endif
    }

    std::cout << "[python] no interpreter found on PATH" << std::endl;
    return false;
}

PythonRunResult PythonRuntime::runProcess(const std::vector<std::string>& cmd, const std::string& cwd)
{
    PythonRunResult r;
    if (!detect() || interp_.empty())
    {
        r.error = "Python interpreter not found. Install Python 3 and add it to PATH.";
        return r;
    }

    std::ostringstream oss;
#ifdef _WIN32
    oss << "\"" << interp_ << "\"";
    for (size_t i = 0; i < cmd.size(); ++i)
        oss << " \"" << cmd[i] << "\"";
    /* capture stdout+stderr */
    std::string full = oss.str() + " 2>&1";
    if (!cwd.empty())
    {
        /* cd /d then run */
        full = "cd /d \"" + cwd + "\" && " + full;
    }
    FILE* pipe = _popen(full.c_str(), "r");
#else
    oss << "\"" << interp_ << "\"";
    for (const auto& a : cmd)
        oss << " \"" << a << "\"";
    std::string full = oss.str() + " 2>&1";
    if (!cwd.empty())
        full = "cd \"" + cwd + "\" && " + full;
    FILE* pipe = popen(full.c_str(), "r");
#endif
    if (!pipe)
    {
        r.error = "failed to start Python process";
        return r;
    }
    char buffer[256];
    while (std::fgets(buffer, sizeof(buffer), pipe))
        r.output += buffer;
#ifdef _WIN32
    r.exitCode = _pclose(pipe);
#else
    int st = pclose(pipe);
    r.exitCode = st;
#endif
    r.ok = (r.exitCode == 0);
    return r;
}

PythonRunResult PythonRuntime::runFile(const std::string& scriptPath,
                                       const std::string& cwd,
                                       const std::vector<std::string>& args)
{
    std::vector<std::string> cmd;
    cmd.push_back(scriptPath);
    for (const auto& a : args)
        cmd.push_back(a);
    return runProcess(cmd, cwd);
}

PythonRunResult PythonRuntime::runCode(const std::string& code, const std::string& cwd)
{
    return runProcess({"-c", code}, cwd);
}

bool PythonRuntime::installPackageDir(const std::string& sourceDir, std::string& outId)
{
    if (!stdfs::is_directory(sourceDir)) return false;
    auto& vfs = velora::fs::VelFS::get();
    if (vfs.hostRoot().empty()) vfs.mount();

    stdfs::path src(sourceDir);
    outId = src.filename().string();
    std::string destV = "/Applications/User/" + outId;
    stdfs::path dest = vfs.hostPath(destV);
    std::error_code ec;
    stdfs::create_directories(dest.parent_path(), ec);
    stdfs::copy(src, dest, stdfs::copy_options::recursive | stdfs::copy_options::overwrite_existing, ec);
    return !ec;
}
} // namespace velora::system
