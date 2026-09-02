#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace velora::fs
{
struct Node {
    std::string name;
    std::string path;
    bool isDir = true;
    std::size_t size = 0;
    std::int64_t mtime = 0; /* unix seconds */
    std::string ext;        /* lower, without dot */
};

class VelFS {
public:
    static VelFS& get();
    bool mount(const std::string& preferred = "");
    const std::string& hostRoot() const { return hostRoot_; }

    std::vector<Node> list(const std::string& vpath) const;
    bool exists(const std::string& vpath) const;
    bool isDir(const std::string& vpath) const;
    bool mkdir(const std::string& vpath);
    bool writeText(const std::string& vpath, const std::string& text);
    std::string readText(const std::string& vpath) const;
    bool remove(const std::string& vpath);
    bool rename(const std::string& from, const std::string& to);
    bool copy(const std::string& from, const std::string& to);
    bool move(const std::string& from, const std::string& to);
    std::string hostPath(const std::string& vpath) const;
    static std::string parent(const std::string& vpath);
    static std::string join(const std::string& dir, const std::string& name);

private:
    std::string hostRoot_;
    void ensureLayout();
};
} // namespace velora::fs
