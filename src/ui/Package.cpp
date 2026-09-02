#include "ui/Package.hpp"

#include "ui/GlCompat.hpp"
#include "third_party/stb_image.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

namespace velora::ui
{
/* Minimal JSON field extractor (no full parser dependency) */
static std::string jsonString(const std::string& src, const std::string& key)
{
    const std::string pat = "\"" + key + "\"";
    size_t p = src.find(pat);
    if (p == std::string::npos) return {};
    p = src.find(':', p + pat.size());
    if (p == std::string::npos) return {};
    p = src.find('"', p + 1);
    if (p == std::string::npos) return {};
    size_t e = src.find('"', p + 1);
    if (e == std::string::npos) return {};
    return src.substr(p + 1, e - p - 1);
}

PackageManager& PackageManager::get()
{
    static PackageManager pm;
    return pm;
}

unsigned PackageManager::loadIcon(const std::string& path, int& w, int& h)
{
    stbi_set_flip_vertically_on_load(0);
    int comp = 0;
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

bool PackageManager::loadOne(const std::string& folder)
{
    fs::path dir(folder);
    fs::path man = dir / "manifest.json";
    if (!fs::is_regular_file(man)) return false;

    std::ifstream in(man);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    const std::string raw = ss.str();

    AppPackage p;
    p.folderPath = dir.string();
    p.id = dir.filename().string();
    p.packageName = jsonString(raw, "package");
    p.name = jsonString(raw, "name");
    if (p.name.empty()) p.name = p.id;
    p.version = jsonString(raw, "version");
    p.developer = jsonString(raw, "developer");
    p.entry = jsonString(raw, "entry");
    if (p.entry.empty()) p.entry = "native:" + p.id;
    std::string iconRel = jsonString(raw, "icon");
    if (iconRel.empty()) iconRel = "icon.png";
    p.iconPath = (dir / iconRel).string();
    p.type = jsonString(raw, "type");
    if (p.type.empty())
        p.type = (p.entry.find(".py") != std::string::npos) ? "python" : "system";
    p.system = (p.type != "user" && p.type != "python");


    if (fs::is_regular_file(p.iconPath))
        p.iconTex = loadIcon(p.iconPath, p.iconW, p.iconH);

    packages_.push_back(std::move(p));
    return true;
}

bool PackageManager::scan(const std::string& mediaRootHint)
{
    packages_.clear();
    const char* roots[] = {"apps", "../apps", "../../apps", "./apps"};
    std::string found;
    for (const char* r : roots)
    {
        if (fs::is_directory(r))
        {
            found = r;
            break;
        }
    }
    if (found.empty() && !mediaRootHint.empty())
    {
        fs::path alt = fs::path(mediaRootHint).parent_path() / "apps";
        if (fs::is_directory(alt)) found = alt.string();
    }
    if (found.empty())
    {
        std::cerr << "[packages] apps/ not found\n";
        return false;
    }

    auto scanDir = [&](const std::string& root) {
        if (!fs::is_directory(root)) return;
        std::cout << "[packages] scanning " << root << std::endl;
        for (auto& e : fs::directory_iterator(root))
        {
            if (!e.is_directory()) continue;
            if (loadOne(e.path().string()))
                std::cout << "  + " << e.path().filename().string() << std::endl;
        }
    };
    scanDir(found);
    /* User Python / vel packages from VelFS */
    for (const char* extra : {
             "VelFS/Applications/User", "VelFS/Applications/System",
             "../VelFS/Applications/User"})
        scanDir(extra);
    return !packages_.empty();
}

const AppPackage* PackageManager::find(const std::string& id) const
{
    for (const auto& p : packages_)
        if (p.id == id || p.packageName == id) return &p;
    return nullptr;
}

void PackageManager::shutdown()
{
    for (auto& p : packages_)
        if (p.iconTex)
        {
            glDeleteTextures(1, &p.iconTex);
            p.iconTex = 0;
        }
    packages_.clear();
}
} // namespace velora::ui
