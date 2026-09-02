#pragma once

#include <string>
#include <unordered_map>

namespace velora::ui
{
/* Online Material Symbols with on-disk cache under VelFS/System/Cache */
class IconCache
{
public:
    static IconCache& get();

    /* Download MaterialIcons-Regular.ttf once into cache; returns local path or empty */
    std::string ensureIconFont();

    /* Load (download if needed) a Material Symbol as PNG texture. name e.g. "settings" */
    unsigned texture(const std::string& name, int px = 64);
    void shutdown();

private:
    std::unordered_map<std::string, unsigned> tex_;
    std::string cacheRoot_;
    bool resolveCacheRoot();
    bool downloadFile(const std::string& url, const std::string& destPath);
    unsigned loadPng(const std::string& path);
};
} // namespace velora::ui
