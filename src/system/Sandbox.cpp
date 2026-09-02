#include "system/Sandbox.hpp"
#include "fs/VelFS.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#endif

namespace stdfs = std::filesystem;

namespace velora::system
{
BrowserSandbox& BrowserSandbox::get()
{
    static BrowserSandbox s;
    return s;
}

bool BrowserSandbox::prepare()
{
    auto& vfs = velora::fs::VelFS::get();
    if (vfs.hostRoot().empty())
        vfs.mount();

    cacheDir_ = (stdfs::path(vfs.hostRoot()) / "System" / "Cache" / "clor-sandbox").string();
    stdfs::create_directories(stdfs::path(cacheDir_) / "profile");
    stdfs::create_directories(stdfs::path(cacheDir_) / "tmp");
    stdfs::create_directories(stdfs::path(cacheDir_) / "downloads");

    std::ofstream mark(stdfs::path(cacheDir_) / "SANDBOX.txt");
    mark << "Clor browser sandbox\n"
         << "- Separate process from VeloraOS\n"
         << "- Profile only here\n"
         << "- http(s) only\n"
         << "- Downloads: downloads/\n";

    lastError_.clear();
    return true;
}

bool BrowserSandbox::navigateFallback(const std::string& url)
{
#ifdef _WIN32
    HINSTANCE hi = ShellExecuteA(nullptr, "open", url.c_str(), nullptr, cacheDir_.c_str(), SW_SHOWNORMAL);
    if ((INT_PTR)hi <= 32)
    {
        lastError_ = "ShellExecute failed";
        return false;
    }
    return true;
#else
    return std::system(("xdg-open \"" + url + "\"").c_str()) == 0;
#endif
}

bool BrowserSandbox::navigateWindows(const std::string& url)
{
#ifdef _WIN32
    prepare();
    std::string profile = (stdfs::path(cacheDir_) / "profile").string();
    std::string downloads = (stdfs::path(cacheDir_) / "downloads").string();

    const char* browsers[] = {
        "C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe",
        "C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe",
        "C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe",
        "C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe",
        "msedge",
        "chrome",
    };

    for (const char* br : browsers)
    {
        /* App window + isolated profile = simplest strong sandbox boundary */
        std::string cmdLine = std::string("\"") + br +
            "\" --user-data-dir=\"" + profile +
            "\" --download-default-directory=\"" + downloads +
            "\" --disable-extensions --disable-plugins --disable-background-networking"
            " --no-first-run --no-default-browser-check --disable-features=TranslateUI"
            " --app=\"" + url + "\"";

        std::vector<char> buf(cmdLine.begin(), cmdLine.end());
        buf.push_back('\0');

        STARTUPINFOA si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};

        if (CreateProcessA(nullptr, buf.data(), nullptr, nullptr, FALSE,
                           0,
                           nullptr, cacheDir_.c_str(), &si, &pi))
        {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            std::cout << "[sandbox] Clor engine -> " << url << std::endl;
            lastError_.clear();
            return true;
        }
    }

    lastError_ = "Edge/Chrome not found";
    return navigateFallback(url);
#else
    (void)url;
    return false;
#endif
}

bool BrowserSandbox::navigate(const std::string& url)
{
    if (url.empty())
    {
        lastError_ = "empty url";
        return false;
    }
    if (!(url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0))
    {
        lastError_ = "only http:// and https://";
        return false;
    }
    prepare();
#ifdef _WIN32
    return navigateWindows(url);
#else
    return navigateFallback(url);
#endif
}
} // namespace velora::system
