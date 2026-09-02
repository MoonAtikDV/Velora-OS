#pragma once

#include "imgui.h"

#include <string>
#include <vector>

namespace velora::ui
{
struct Wallpaper
{
    std::string path;
    unsigned texture = 0;
    unsigned blurTexture = 0; /* mild blur for taskbar acrylic */
    int width = 0;
    int height = 0;
    float avgR = 0.4f, avgG = 0.3f, avgB = 0.6f;
    unsigned thumbTexture = 0;
    int thumbW = 0, thumbH = 0;
};

class Assets
{
public:
    static Assets& get();

    bool discover();
    const std::string& mediaRoot() const { return mediaRoot_; }

    bool loadFonts();
    bool loadWallpapers();

    ImFont* fontUi() const { return fontUi_; }
    ImFont* fontIcons() const { return fontIcons_; }

    const std::vector<Wallpaper>& wallpapers() const { return wallpapers_; }
    const Wallpaper* wallpaperAt(int index1based) const;

    void applyAccentFromWallpaper(int index1based);
    unsigned loadTextureFile(const std::string& path, int& w, int& h);

    void shutdown();

private:
    std::string mediaRoot_;
    ImFont* fontUi_ = nullptr;
    ImFont* fontIcons_ = nullptr;
    std::vector<Wallpaper> wallpapers_;

    bool tryMediaRoot(const std::string& path);
    unsigned uploadRGBA(const unsigned char* data, int w, int h);
    unsigned loadTexture2D(const std::string& path, int& w, int& h, unsigned char** outPixels = nullptr);
    unsigned makeMildBlur(const unsigned char* src, int w, int h, int radius = 4);
};
} // namespace velora::ui
