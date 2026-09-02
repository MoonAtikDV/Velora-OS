#include "ui/Assets.hpp"
#include "ui/Theme.hpp"
#include "ui/IconCache.hpp"

#include "ui/GlCompat.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <vector>

namespace fs = std::filesystem;

namespace velora::ui
{
Assets& Assets::get()
{
    static Assets a;
    return a;
}

bool Assets::tryMediaRoot(const std::string& path)
{
    fs::path p(path);
    if (fs::is_directory(p / "wallpapers") || fs::is_directory(p / "fonts"))
    {
        mediaRoot_ = p.string();
        return true;
    }
    return false;
}

bool Assets::discover()
{
    const char* candidates[] = {"media", "../media", "../../media", "./media"};
    for (const char* c : candidates)
        if (tryMediaRoot(c))
        {
            std::cout << "[assets] media root: " << mediaRoot_ << "\n";
            return true;
        }
    fs::create_directories("media/wallpapers");
    fs::create_directories("media/fonts/OpenSans");
    fs::create_directories("media/fonts/MaterialSymbols");
    mediaRoot_ = "media";
    std::cout << "[assets] using ./media\n";
    return false;
}

unsigned Assets::uploadRGBA(const unsigned char* data, int w, int h)
{
    unsigned tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    return tex;
}

unsigned Assets::loadTexture2D(const std::string& path, int& w, int& h, unsigned char** outPixels)
{
    stbi_set_flip_vertically_on_load(0);
    int comp = 0;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &comp, 4);
    if (!data) return 0;
    unsigned tex = uploadRGBA(data, w, h);
    if (outPixels)
        *outPixels = data;
    else
        stbi_image_free(data);
    return tex;
}

/* Separable box blur — intentionally mild (Windows acrylic-ish soft) */
unsigned Assets::makeMildBlur(const unsigned char* src, int w, int h, int radius)
{
    if (!src || w <= 0 || h <= 0) return 0;
    /* Downscale first — blur must stay cheap (was freezing on large images) */
    const int maxW = 320;
    int dw = w, dh = h;
    if (dw > maxW)
    {
        dh = std::max(1, h * maxW / w);
        dw = maxW;
    }
    std::vector<unsigned char> small((size_t)dw * dh * 4);
    for (int y = 0; y < dh; ++y)
    {
        int sy = y * h / dh;
        for (int x = 0; x < dw; ++x)
        {
            int sx = x * w / dw;
            for (int c = 0; c < 4; ++c)
                small[(y * dw + x) * 4 + c] = src[(sy * w + sx) * 4 + c];
        }
    }
    radius = std::max(1, std::min(radius, 4));
    std::vector<unsigned char> tmp((size_t)dw * dh * 4);
    std::vector<unsigned char> dst((size_t)dw * dh * 4);
    w = dw;
    h = dh;
    src = small.data();

    auto sample = [&](const unsigned char* img, int x, int y, int c) -> int {
        x = std::clamp(x, 0, w - 1);
        y = std::clamp(y, 0, h - 1);
        return img[(y * w + x) * 4 + c];
    };

    /* horizontal */
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            for (int c = 0; c < 4; ++c)
            {
                int sum = 0, n = 0;
                for (int k = -radius; k <= radius; ++k)
                {
                    sum += sample(src, x + k, y, c);
                    ++n;
                }
                tmp[(y * w + x) * 4 + c] = (unsigned char)(sum / n);
            }
    /* vertical */
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            for (int c = 0; c < 4; ++c)
            {
                int sum = 0, n = 0;
                for (int k = -radius; k <= radius; ++k)
                {
                    sum += sample(tmp.data(), x, y + k, c);
                    ++n;
                }
                dst[(y * w + x) * 4 + c] = (unsigned char)(sum / n);
            }

    return uploadRGBA(dst.data(), w, h);
}

bool Assets::loadWallpapers()
{
    wallpapers_.clear();
    fs::path dir = fs::path(mediaRoot_) / "wallpapers";
    if (!fs::is_directory(dir)) return false;

    std::vector<fs::path> files;
    for (auto& e : fs::directory_iterator(dir))
    {
        if (!e.is_regular_file()) continue;
        auto ext = e.path().extension().string();
        for (char& c : ext)
            c = (char)tolower((unsigned char)c);
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp")
            files.push_back(e.path());
    }
    std::sort(files.begin(), files.end());

    for (const auto& f : files)
    {
        Wallpaper w;
        w.path = f.string();
        unsigned char* pixels = nullptr;
        w.texture = loadTexture2D(w.path, w.width, w.height, &pixels);
        if (w.texture && pixels)
        {
            /* Weighted average — prefer more saturated pixels for a readable accent */
            double ar = 0, ag = 0, ab = 0, weightSum = 0;
            const int step = std::max(1, std::min(w.width, w.height) / 48);
            for (int y = 0; y < w.height; y += step)
                for (int x = 0; x < w.width; x += step)
                {
                    const unsigned char* px = pixels + (y * w.width + x) * 4;
                    float rf = px[0] / 255.f, gf = px[1] / 255.f, bf = px[2] / 255.f;
                    float mx = std::max(rf, std::max(gf, bf));
                    float mn = std::min(rf, std::min(gf, bf));
                    float sat = (mx > 1e-4f) ? (mx - mn) / mx : 0.f;
                    float wt = 0.35f + sat * 1.65f; /* saturated pixels dominate */
                    ar += rf * wt; ag += gf * wt; ab += bf * wt;
                    weightSum += wt;
                }
            if (weightSum > 0)
            {
                w.avgR = (float)(ar / weightSum);
                w.avgG = (float)(ag / weightSum);
                w.avgB = (float)(ab / weightSum);
                float m = (w.avgR + w.avgG + w.avgB) / 3.f;
                w.avgR = std::clamp(m + (w.avgR - m) * 1.55f, 0.08f, 0.95f);
                w.avgG = std::clamp(m + (w.avgG - m) * 1.55f, 0.08f, 0.95f);
                w.avgB = std::clamp(m + (w.avgB - m) * 1.55f, 0.08f, 0.95f);
            }
            w.blurTexture = makeMildBlur(pixels, w.width, w.height, 3);
            {
                const int maxW = 160;
                int tw = w.width, th = w.height;
                if (tw > maxW) { th = std::max(1, w.height * maxW / w.width); tw = maxW; }
                std::vector<unsigned char> small((size_t)tw * th * 4);
                for (int y = 0; y < th; ++y) {
                    int sy = y * w.height / th;
                    for (int x = 0; x < tw; ++x) {
                        int sx = x * w.width / tw;
                        for (int c = 0; c < 4; ++c)
                            small[(y * tw + x) * 4 + c] = pixels[(sy * w.width + sx) * 4 + c];
                    }
                }
                w.thumbW = tw; w.thumbH = th;
                w.thumbTexture = uploadRGBA(small.data(), tw, th);
            }
            stbi_image_free(pixels);
            wallpapers_.push_back(w);
            std::cout << "[assets] wallpaper: " << f.filename().string()
                      << " (" << w.width << "x" << w.height << ") +blur\n";
        }
        else if (pixels)
            stbi_image_free(pixels);
    }
    return !wallpapers_.empty();
}

bool Assets::loadFonts()
{
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    const float size = 16.0f;
    const float iconSize = 18.0f;

    fs::path openSansDir = fs::path(mediaRoot_) / "fonts" / "OpenSans";
    std::string openSansFile;
    if (fs::is_directory(openSansDir))
    {
        for (auto& e : fs::directory_iterator(openSansDir))
        {
            auto lower = e.path().filename().string();
            for (char& c : lower)
                c = (char)tolower((unsigned char)c);
            if ((lower.find("regular") != std::string::npos || openSansFile.empty()) &&
                (lower.size() > 4) &&
                (lower.rfind(".ttf") == lower.size() - 4 || lower.rfind(".otf") == lower.size() - 4))
            {
                if (lower.find("regular") != std::string::npos || openSansFile.empty())
                    openSansFile = e.path().string();
                if (lower.find("regular") != std::string::npos)
                    break;
            }
        }
    }

    if (!openSansFile.empty())
    {
        fontUi_ = io.Fonts->AddFontFromFileTTF(openSansFile.c_str(), size);
        std::cout << "[assets] UI font: " << openSansFile << "\n";
    }
    else
        fontUi_ = io.Fonts->AddFontDefault();

    std::string msFile;
    const char* fontDirs[] = {"MaterialSymbols", "MaterialIcons", "material-icons", "material-symbols", "font"};
    for (const char* sub : fontDirs)
    {
        fs::path msDir = fs::path(mediaRoot_) / "fonts" / sub;
        if (!fs::is_directory(msDir)) continue;
        for (auto& e : fs::directory_iterator(msDir))
        {
            if (!e.is_regular_file()) continue;
            auto ext = e.path().extension().string();
            for (char& c : ext) c = (char)tolower((unsigned char)c);
            if (ext != ".ttf" && ext != ".otf") continue;
            auto lower = e.path().filename().string();
            for (char& c : lower) c = (char)tolower((unsigned char)c);
            if (lower.find("materialicons-regular") != std::string::npos) {
                msFile = e.path().string(); break;
            }
            if (msFile.empty())
                msFile = e.path().string();
        }
        if (!msFile.empty() && msFile.find("MaterialIcons-Regular") != std::string::npos) break;
    }
    if (msFile.empty())
    {
        std::string cached = IconCache::get().ensureIconFont();
        if (!cached.empty()) msFile = cached;
    }

    if (!msFile.empty())
    {
        ImFontConfig cfg;
        cfg.PixelSnapH = true;
        static const ImWchar ranges[] = { 0x0020, 0x00FF, 0xE000, 0xF8FF, 0 };
        cfg.GlyphMinAdvanceX = iconSize;
        fontIcons_ = io.Fonts->AddFontFromFileTTF(msFile.c_str(), iconSize, &cfg, ranges);
        std::cout << "[assets] icon font: " << msFile << std::endl;
    }
    else
        fontIcons_ = fontUi_;

    io.Fonts->Build();
    return true;
}

const Wallpaper* Assets::wallpaperAt(int index1based) const
{
    if (wallpapers_.empty()) return nullptr;
    int i = (index1based - 1) % (int)wallpapers_.size();
    if (i < 0) i += (int)wallpapers_.size();
    return &wallpapers_[i];
}

unsigned Assets::loadTextureFile(const std::string& path, int& w, int& h)
{
    return loadTexture2D(path, w, h, nullptr);
}

void Assets::applyAccentFromWallpaper(int index1based)
{
    const Wallpaper* wp = wallpaperAt(index1based);
    if (!wp)
    {
        std::cout << "[assets] accent: no wallpaper at index " << index1based << std::endl;
        return;
    }
    auto& theme = Theme::get();
    float r = wp->avgR, g = wp->avgG, b = wp->avgB;
    /* Lift dark averages so accent is visible on UI chrome */
    float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
    if (lum < 0.25f)
    {
        float boost = 0.35f / std::max(lum, 0.05f);
        r = std::min(1.f, r * boost);
        g = std::min(1.f, g * boost);
        b = std::min(1.f, b * boost);
    }
    theme.accent = ImVec4(r, g, b, 1.f);
    theme.accentHover = ImVec4(std::min(1.f, r * 1.15f), std::min(1.f, g * 1.15f), std::min(1.f, b * 1.15f), 1.f);
    theme.selection = ImVec4(r, g, b, 0.35f);
    theme.bgTitle = ImVec4(r, g, b, 0.22f);
    theme.applyImGuiStyle();
    std::cout << "[assets] accent from wallpaper #" << index1based
              << " rgb(" << r << "," << g << "," << b << ")" << std::endl;
}

void Assets::shutdown()
{
    for (auto& w : wallpapers_)
    {
        if (w.texture) glDeleteTextures(1, &w.texture);
        if (w.blurTexture) glDeleteTextures(1, &w.blurTexture);
        if (w.thumbTexture) glDeleteTextures(1, &w.thumbTexture);
        w.texture = w.blurTexture = w.thumbTexture = 0;
    }
    wallpapers_.clear();
}
} // namespace velora::ui
