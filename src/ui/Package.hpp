#pragma once

#include <string>
#include <vector>

namespace velora::ui
{
struct AppPackage
{
    std::string id;          /* folder name */
    std::string packageName; /* package field */
    std::string name;
    std::string version;
    std::string developer;
    std::string entry;       /* native:id or path */
    std::string folderPath;
    std::string iconPath;
    unsigned iconTex = 0;
    int iconW = 0;
    int iconH = 0;
    bool system = true;
    std::string type; /* system | user | python */
    bool isPython() const {
        if (type == "python") return true;
        if (entry.size() >= 3 && entry.substr(entry.size() - 3) == ".py") return true;
        return false;
    }
};


class PackageManager
{
public:
    static PackageManager& get();

    bool scan(const std::string& mediaRootHint = "");
    const std::vector<AppPackage>& all() const { return packages_; }
    const AppPackage* find(const std::string& id) const;
    void shutdown();

private:
    std::vector<AppPackage> packages_;
    bool loadOne(const std::string& folder);
    unsigned loadIcon(const std::string& path, int& w, int& h);
};
} // namespace velora::ui
