#pragma once

#include <string>
#include <vector>

namespace velora::system
{
struct PythonRunResult
{
    bool ok = false;
    int exitCode = -1;
    std::string output;
    std::string error;
};

/**
 * Host Python integration for VeloraOS packages.
 * Discovers python/python3 on PATH and runs scripts in a sandbox cwd.
 */
class PythonRuntime
{
public:
    static PythonRuntime& get();

    /** Find interpreter once; returns path or empty */
    bool detect();
    const std::string& interpreter() const { return interp_; }
    bool available() const { return !interp_.empty(); }

    /** Run a .py file; cwd = working directory (package folder) */
    PythonRunResult runFile(const std::string& scriptPath,
                            const std::string& cwd,
                            const std::vector<std::string>& args = {});

    /** Run -c code */
    PythonRunResult runCode(const std::string& code, const std::string& cwd = "");

    /** Install/copy a folder package into VelFS Applications/User */
    bool installPackageDir(const std::string& sourceDir, std::string& outId);

private:
    std::string interp_;
    bool detected_ = false;

    PythonRunResult runProcess(const std::vector<std::string>& cmd, const std::string& cwd);
};
} // namespace velora::system
