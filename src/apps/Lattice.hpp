#pragma once
#include "fs/VelFS.hpp"
#include <string>
#include <vector>

namespace velora::apps
{
enum class LatticeView { Details, Icons };
enum class LatticeSort { Name, Size, MTime, Type };

struct LatticePane
{
    std::string cwd = "/User";
    std::vector<std::string> history;
    int histIdx = -1;
    std::vector<std::string> selected;
    std::string focusPath;
};

class Lattice
{
public:
    static Lattice& get();
    void draw();
    void requestOpen(const std::string& vpath);

private:
    LatticePane left_;
    LatticePane right_;
    bool dual_ = false;
    bool preview_ = true;
    LatticeView view_ = LatticeView::Details;
    LatticeSort sort_ = LatticeSort::Name;
    bool sortAsc_ = true;
    char filter_[128]{};
    char renameBuf_[256]{};
    char newNameBuf_[128]{"NewFolder"};
    bool showHidden_ = false;
    std::string clipPath_;
    bool clipCut_ = false;
    std::string status_;
    std::string previewText_;
    std::string previewPath_;
    LatticePane* active_ = &left_;
    std::string pendingOpen_;

    void navigate(LatticePane& p, const std::string& path, bool pushHist = true);
    void goUp(LatticePane& p);
    void goBack(LatticePane& p);
    void goForward(LatticePane& p);
    std::vector<velora::fs::Node> sortedList(const LatticePane& p) const;
    void drawSidebar();
    void drawToolbar();
    void drawPane(LatticePane& p, const char* id, float width);
    void drawPreview();
    void drawStatus();
    void openNode(LatticePane& p, const velora::fs::Node& n);
    void applyClipboard(LatticePane& p);
    void deleteSelected(LatticePane& p);
    void refreshPreview(const std::string& path, bool isDir);
};
} // namespace velora::apps
