#include "fs/VelFS.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <system_error>

namespace stdfs = std::filesystem;

namespace velora::fs
{
VelFS& VelFS::get() { static VelFS v; return v; }

void VelFS::ensureLayout()
{
    const char* dirs[] = {
        "System", "System/Config", "System/Logs", "System/Cache",
        "User", "User/Documents", "User/Downloads", "User/Pictures", "User/Desktop",
        "Applications", "Applications/System", "Applications/User"
    };
    for (const char* d : dirs)
        stdfs::create_directories(stdfs::path(hostRoot_) / d);
    auto welcome = stdfs::path(hostRoot_) / "User" / "Documents" / "Welcome.txt";
    if (!stdfs::exists(welcome))
    {
        std::ofstream out(welcome);
        out << "Welcome to VeloraOS Lattice\nYour files live under VelFS.\n";
    }
    auto readme = stdfs::path(hostRoot_) / "User" / "Downloads" / "README.txt";
    if (!stdfs::exists(readme))
    {
        std::ofstream out(readme);
        out << "Downloads folder\n";
    }
}

bool VelFS::mount(const std::string&)
{
    hostRoot_ = "VelFS";
    stdfs::create_directories(hostRoot_);
    ensureLayout();
    std::cout << "[VelFS] mounted at " << stdfs::absolute(hostRoot_) << std::endl;
    return true;
}

std::string VelFS::hostPath(const std::string& vpath) const
{
    std::string p = vpath;
    if (p.empty() || p[0] != '/') p = "/" + p;
    std::string rel = p.substr(1);
    return (stdfs::path(hostRoot_) / rel).string();
}

std::string VelFS::parent(const std::string& vpath)
{
    if (vpath.empty() || vpath == "/") return "/";
    std::string p = vpath;
    while (p.size() > 1 && p.back() == '/') p.pop_back();
    auto pos = p.find_last_of('/');
    if (pos == std::string::npos || pos == 0) return "/";
    return p.substr(0, pos);
}

std::string VelFS::join(const std::string& dir, const std::string& name)
{
    if (dir.empty() || dir == "/") return "/" + name;
    if (dir.back() == '/') return dir + name;
    return dir + "/" + name;
}

std::vector<Node> VelFS::list(const std::string& vpath) const
{
    std::vector<Node> out;
    stdfs::path hp = hostPath(vpath);
    if (!stdfs::is_directory(hp)) return out;
    std::error_code ec;
    for (auto& e : stdfs::directory_iterator(hp, ec))
    {
        Node n;
        n.name = e.path().filename().string();
        n.path = join(vpath == "/" ? "/" : vpath, n.name);
        if (vpath == "/") n.path = "/" + n.name;
        else n.path = join(vpath, n.name);
        n.isDir = e.is_directory(ec);
        if (!n.isDir)
        {
            n.size = (std::size_t)stdfs::file_size(e.path(), ec);
            auto ext = e.path().extension().string();
            if (!ext.empty() && ext[0] == '.') ext = ext.substr(1);
            for (char& c : ext) c = (char)tolower((unsigned char)c);
            n.ext = ext;
        }
        {
            auto ftime = stdfs::last_write_time(e.path(), ec);
            if (!ec)
            {
                try
                {
                    auto sctp = std::chrono::time_point_cast<std::chrono::seconds>(
                        ftime - stdfs::file_time_type::clock::now() + std::chrono::system_clock::now());
                    n.mtime = (std::int64_t)sctp.time_since_epoch().count();
                }
                catch (...) { n.mtime = 0; }
            }
        }
        out.push_back(std::move(n));
    }
    return out;
}

bool VelFS::exists(const std::string& vpath) const { return stdfs::exists(hostPath(vpath)); }
bool VelFS::isDir(const std::string& vpath) const { return stdfs::is_directory(hostPath(vpath)); }

bool VelFS::mkdir(const std::string& vpath)
{
    return stdfs::create_directories(hostPath(vpath));
}

bool VelFS::writeText(const std::string& vpath, const std::string& text)
{
    stdfs::path hp = hostPath(vpath);
    stdfs::create_directories(hp.parent_path());
    std::ofstream out(hp, std::ios::binary);
    if (!out) return false;
    out << text;
    return true;
}

std::string VelFS::readText(const std::string& vpath) const
{
    std::ifstream in(hostPath(vpath), std::ios::binary);
    if (!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool VelFS::remove(const std::string& vpath)
{
    std::error_code ec;
    return stdfs::remove_all(hostPath(vpath), ec) > 0;
}

bool VelFS::rename(const std::string& from, const std::string& to)
{
    std::error_code ec;
    stdfs::rename(hostPath(from), hostPath(to), ec);
    return !ec;
}

bool VelFS::copy(const std::string& from, const std::string& to)
{
    std::error_code ec;
    auto opts = stdfs::copy_options::recursive | stdfs::copy_options::overwrite_existing;
    stdfs::copy(hostPath(from), hostPath(to), opts, ec);
    return !ec;
}

bool VelFS::move(const std::string& from, const std::string& to)
{
    if (rename(from, to)) return true;
    if (!copy(from, to)) return false;
    return remove(from);
}
} // namespace velora::fs
