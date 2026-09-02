#pragma once

#include <string>
#include <vector>

namespace velora::core { class Kernel; }

namespace velora::apps
{
struct TermLine
{
    std::string text;
    int color = 0; /* 0 default 1 dim 2 ok 3 warn 4 err 5 accent */
};

struct TermTab
{
    std::string title = "nucleus";
    std::string cwd = "/User";
    std::vector<TermLine> lines;
    std::vector<std::string> history;
    int histIdx = -1;
    char input[512]{};
};

class Nucleus
{
public:
    static Nucleus& get();

    void ensure();
    void draw(velora::core::Kernel& kernel);

private:
    std::vector<TermTab> tabs_;
    int active_ = 0;
    bool inited_ = false;

    TermTab& cur();
    void println(TermTab& t, const std::string& s, int color = 0);
    void exec(TermTab& t, velora::core::Kernel& k, const std::string& line);
    void execOne(TermTab& t, velora::core::Kernel& k, const std::string& cmd);
    std::vector<std::string> splitArgs(const std::string& s);
    std::string resolve(TermTab& t, const std::string& path);
};
} // namespace velora::apps
