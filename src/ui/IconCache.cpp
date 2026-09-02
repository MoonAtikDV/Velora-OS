#include "ui/IconCache.hpp"
#include "ui/GlCompat.hpp"
#include "fs/VelFS.hpp"

#include "third_party/stb_image.h"

#include <filesystem>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <urlmon.h>
#pragma comment(lib, "urlmon.lib")
#endif

namespace stdfs = std::filesystem;

namespace velora::ui
{
IconCache& IconCache::get()
{
    static IconCache c;
    return c;
}

bool IconCache::resolveCacheRoot()
{
    if (!cacheRoot_.empty()) return true;
    auto& vfs = velora::fs::VelFS::get();
    if (vfs.hostRoot().empty())
        vfs.mount();
    cacheRoot_ = (stdfs::path(vfs.hostRoot()) / "System" / "Cache").string();
    stdfs::create_directories(stdfs::path(cacheRoot_) / "fonts");
    stdfs::create_directories(stdfs::path(cacheRoot_) / "icons");
    return true;
}

bool IconCache::downloadFile(const std::string& url, const std::string& destPath)
{
    stdfs::create_directories(stdfs::path(destPath).parent_path());
    std::cout << "[IconCache] GET " << url << "\n  -> " << destPath << std::endl;

#ifdef _WIN32
    HRESULT hr = URLDownloadToFileA(nullptr, url.c_str(), destPath.c_str(), 0, nullptr);
    if (SUCCEEDED(hr) && stdfs::exists(destPath) && stdfs::file_size(destPath) > 100)
        return true;
    std::cerr << "[IconCache] URLDownloadToFile failed hr=" << std::hex << hr << std::endl;
#endif

    /* Fallback: curl if present */
    std::string cmd = "curl -fsSL \"" + url + "\" -o \"" + destPath + "\"";
    int rc = std::system(cmd.c_str());
    if (rc == 0 && stdfs::exists(destPath) && stdfs::file_size(destPath) > 100)
        return true;

    std::cerr << "[IconCache] download failed: " << url << std::endl;
    return false;
}

std::string IconCache::ensureIconFont()
{
    resolveCacheRoot();
    stdfs::path dest = stdfs::path(cacheRoot_) / "fonts" / "MaterialIcons-Regular.ttf";
    if (stdfs::exists(dest) && stdfs::file_size(dest) > 10000)
    {
        std::cout << "[IconCache] font cache hit: " << dest << std::endl;
        return dest.string();
    }

    /* Official Google Material Icons font (codepoints = Material Symbols PUA-compatible set) */
    const char* urls[] = {
        "https://github.com/google/material-design-icons/raw/master/font/MaterialIcons-Regular.ttf",
        "https://raw.githubusercontent.com/google/material-design-icons/master/font/MaterialIcons-Regular.ttf",
    };
    for (const char* u : urls)
    {
        if (downloadFile(u, dest.string()))
            return dest.string();
    }
    return {};
}

unsigned IconCache::loadPng(const std::string& path)
{
    int w = 0, h = 0, comp = 0;
    stbi_set_flip_vertically_on_load(0);
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &comp, 4);
    if (!data) return 0;
    unsigned tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    stbi_image_free(data);
    return tex;
}

unsigned IconCache::texture(const std::string& name, int px)
{
    if (name.empty()) return 0;
    auto it = tex_.find(name);
    if (it != tex_.end()) return it->second;

    resolveCacheRoot();
    stdfs::path dest = stdfs::path(cacheRoot_) / "icons" / (name + ".png");
    if (!stdfs::exists(dest) || stdfs::file_size(dest) < 50)
    {
        /* Iconify CDN — Material Symbols as PNG (cached forever after first fetch) */
        std::string url = "https://api.iconify.design/material-symbols:" + name + ".png?height=" +
                          std::to_string(px);
        if (!downloadFile(url, dest.string()))
        {
            tex_[name] = 0;
            return 0;
        }
    }
    else
    {
        std::cout << "[IconCache] icon cache hit: " << name << std::endl;
    }

    unsigned t = loadPng(dest.string());
    tex_[name] = t;
    return t;
}

void IconCache::shutdown()
{
    for (auto& kv : tex_)
        if (kv.second) glDeleteTextures(1, &kv.second);
    tex_.clear();
}
} // namespace velora::ui
