#pragma once

#include <string>
#include <vector>

namespace velora::apps
{
struct ClorTab
{
    std::string id;
    std::string title;
    std::string url;
    std::vector<std::string> back;
    std::vector<std::string> forward;
    bool loading = false;
    std::string pageText;
    std::string pageError;
};

struct ClorBookmark
{
    std::string title;
    std::string url;
};

class ClorEngine
{
public:
    static ClorEngine& get();

    void ensureDefaults();
    void loadPersistent();
    void savePersistent();

    int active() const { return active_; }
    std::vector<ClorTab>& tabs() { return tabs_; }
    const std::vector<ClorTab>& tabs() const { return tabs_; }
    std::vector<ClorBookmark>& bookmarks() { return bookmarks_; }

    ClorTab* current();
    int newTab(const std::string& url = "https://duckduckgo.com/");
    void closeTab(int index);
    void setActive(int index);

    bool navigate(const std::string& url);
    bool goBack();
    bool goForward();
    bool reload();
    bool openExternal(const std::string& url);

    const std::string& status() const { return status_; }
    void setStatus(const std::string& s) { status_ = s; }
    const std::string& sandboxPath() const { return sandboxPath_; }

private:
    std::vector<ClorTab> tabs_;
    std::vector<ClorBookmark> bookmarks_;
    int active_ = 0;
    std::string status_ = "Ready";
    std::string sandboxPath_;
    int tabSeq_ = 1;

    bool isSafeUrl(const std::string& url) const;
    void pushHistory(ClorTab& t, const std::string& url);
    bool fetchPage(ClorTab& t, const std::string& url);
};
} // namespace velora::apps
