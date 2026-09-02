#include "apps/Lattice.hpp"
#include "system/Notifications.hpp"
#include "core/Kernel.hpp"
#include "core/Access.hpp"
#include "ui/Theme.hpp"
#include "ui/widgets/AppChrome.hpp"
#include "ui/widgets/Forms.hpp"

#include "imgui.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace velora::apps
{
Lattice& Lattice::get()
{
    static Lattice L;
    return L;
}

void Lattice::navigate(LatticePane& p, const std::string& path, bool pushHist)
{
    if (!velora::fs::VelFS::get().isDir(path) && path != "/")
        return;
    if (pushHist)
    {
        if (p.histIdx < (int)p.history.size() - 1 && p.histIdx >= 0)
            p.history.resize(p.histIdx + 1);
        if (p.history.empty() || p.history.back() != p.cwd)
            p.history.push_back(p.cwd);
        p.histIdx = (int)p.history.size();
    }
    p.cwd = path.empty() ? "/" : path;
    p.selected.clear();
    p.focusPath.clear();
    status_ = p.cwd;
}

void Lattice::goUp(LatticePane& p)
{
    navigate(p, velora::fs::VelFS::parent(p.cwd));
}

void Lattice::goBack(LatticePane& p)
{
    if (p.histIdx <= 0 || p.history.empty()) return;
    if (p.histIdx >= (int)p.history.size())
        p.histIdx = (int)p.history.size() - 1;
    else
        p.histIdx--;
    if (p.histIdx >= 0 && p.histIdx < (int)p.history.size())
        navigate(p, p.history[p.histIdx], false);
}

void Lattice::goForward(LatticePane& p)
{
    if (p.histIdx + 1 >= (int)p.history.size()) return;
    p.histIdx++;
    navigate(p, p.history[p.histIdx], false);
}

std::vector<velora::fs::Node> Lattice::sortedList(const LatticePane& p) const
{
    auto list = velora::fs::VelFS::get().list(p.cwd);
    std::string f = filter_;
    for (char& c : f) c = (char)tolower((unsigned char)c);

    list.erase(std::remove_if(list.begin(), list.end(), [&](const velora::fs::Node& n) {
        if (!showHidden_ && !n.name.empty() && n.name[0] == '.') return true;
        if (f.empty()) return false;
        std::string name = n.name;
        for (char& c : name) c = (char)tolower((unsigned char)c);
        return name.find(f) == std::string::npos;
    }), list.end());

    std::sort(list.begin(), list.end(), [&](const velora::fs::Node& a, const velora::fs::Node& b) {
        if (a.isDir != b.isDir) return a.isDir > b.isDir;
        int cmp = 0;
        switch (sort_)
        {
        case LatticeSort::Size:
            cmp = (a.size < b.size) ? -1 : (a.size > b.size) ? 1 : 0;
            break;
        case LatticeSort::MTime:
            cmp = (a.mtime < b.mtime) ? -1 : (a.mtime > b.mtime) ? 1 : 0;
            break;
        case LatticeSort::Type:
            cmp = a.ext.compare(b.ext);
            break;
        default:
            cmp = a.name.compare(b.name);
            break;
        }
        if (cmp == 0) cmp = a.name.compare(b.name);
        return sortAsc_ ? (cmp < 0) : (cmp > 0);
    });
    return list;
}

void Lattice::refreshPreview(const std::string& path, bool isDir)
{
    previewPath_ = path;
    if (isDir)
    {
        auto items = velora::fs::VelFS::get().list(path);
        previewText_ = "Folder · " + std::to_string(items.size()) + " items\n" + path;
        return;
    }
    auto text = velora::fs::VelFS::get().readText(path);
    if (text.size() > 12000)
        text = text.substr(0, 12000) + "\n…";
    if (text.empty())
        previewText_ = "(binary or empty)";
    else
        previewText_ = text;
}

void Lattice::openNode(LatticePane& p, const velora::fs::Node& n)
{
    if (n.isDir)
        navigate(p, n.path);
    else
        refreshPreview(n.path, false);
}

void Lattice::applyClipboard(LatticePane& p)
{
    if (clipPath_.empty()) return;
    if (!velora::core::AccessControl::get().check("files", velora::core::Perm::FilesWrite))
    {
 status_ = "Denied: files.write"; return; }
    auto& vfs = velora::fs::VelFS::get();
    std::string name = clipPath_;
    auto slash = name.find_last_of('/');
    if (slash != std::string::npos) name = name.substr(slash + 1);
    std::string dest = velora::fs::VelFS::join(p.cwd, name);
    bool ok = false;
    if (clipCut_)
        ok = vfs.move(clipPath_, dest);
    else
        ok = vfs.copy(clipPath_, dest);
    status_ = ok ? (clipCut_ ? "Moved" : "Copied") : "Clipboard operation failed";
    if (ok && clipCut_)
    {
        clipPath_.clear();
        clipCut_ = false;
    }
    velora::system::NotificationCenter::get().post("Lattice", status_, "files");
}

void Lattice::deleteSelected(LatticePane& p)
{
    if (!velora::core::AccessControl::get().check("files", velora::core::Perm::FilesWrite))
    {
        status_ = "Denied: files.write";
        velora::system::NotificationCenter::get().post("Access", "files.write required", "system",
            velora::system::NotifLevel::Error);
        return;
    }
    auto& vfs = velora::fs::VelFS::get();
    int n = 0;
    for (const auto& path : p.selected)
    {
        if (vfs.remove(path)) ++n;
    }
    p.selected.clear();
    status_ = "Deleted " + std::to_string(n) + " item(s)";
    velora::system::NotificationCenter::get().post("Lattice", status_, "files",
                                                   velora::system::NotifLevel::Warning);
}

void Lattice::drawSidebar()
{
    using namespace velora::ui::chrome;
    using namespace velora::ui::forms;
    SidebarFrame(188.f);
    ImGui::TextUnformatted("Lattice");
    ImGui::TextDisabled("File system");
    ImGui::Spacing();
    struct Place { const char* label; const char* path; };
    Place places[] = {
        {"Home", "/User"},
        {"Desktop", "/User/Desktop"},
        {"Documents", "/User/Documents"},
        {"Downloads", "/User/Downloads"},
        {"Pictures", "/User/Pictures"},
        {"Applications", "/Applications"},
        {"System", "/System"},
        {"Root", "/"},
    };
    for (auto& pl : places)
    {
        bool sel = (active_->cwd == pl.path);
        if (NavItem(pl.label, sel))
            navigate(*active_, pl.path);
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextDisabled("View");
    if (Chip("Details", view_ == LatticeView::Details)) view_ = LatticeView::Details;
    ImGui::SameLine();
    if (Chip("Icons", view_ == LatticeView::Icons)) view_ = LatticeView::Icons;
    ImGui::Spacing();
    Switch("Dual pane", &dual_);
    Switch("Preview", &preview_);
    Switch("Hidden files", &showHidden_);
    EndSidebar();
}

void Lattice::drawToolbar()
{
    using namespace velora::ui::forms;
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 14.f);
    if (TonalButton("<", ImVec2(36, 32))) goBack(*active_);
    ImGui::SameLine(0, 4);
    if (TonalButton(">", ImVec2(36, 32))) goForward(*active_);
    ImGui::SameLine(0, 4);
    if (TonalButton("^", ImVec2(36, 32))) goUp(*active_);
    ImGui::SameLine(0, 4);
    if (TonalButton("Home", ImVec2(56, 32))) navigate(*active_, "/User");
    ImGui::SameLine(0, 8);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 300);
    static char pathBuf[512];
    std::snprintf(pathBuf, sizeof(pathBuf), "%s", active_->cwd.c_str());
    if (ImGui::InputText("##path", pathBuf, sizeof(pathBuf), ImGuiInputTextFlags_EnterReturnsTrue))
        navigate(*active_, pathBuf);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    ImGui::InputTextWithHint("##filter", "Filter", filter_, sizeof(filter_));
    ImGui::SameLine();
    if (FilledButton("New", ImVec2(64, 32)))
    {
        ImGui::OpenPopup("NewFolder");
        std::snprintf(newNameBuf_, sizeof(newNameBuf_), "NewFolder");
    }
    if (ImGui::BeginPopup("NewFolder"))
    {
        ImGui::InputText("Name", newNameBuf_, sizeof(newNameBuf_));
        if (ImGui::Button("Create"))
        {
            auto path = velora::fs::VelFS::join(active_->cwd, newNameBuf_);
            if (velora::fs::VelFS::get().mkdir(path))
            {
                status_ = "Created " + path;
                velora::system::NotificationCenter::get().post("Lattice", status_, "files",
                                                               velora::system::NotifLevel::Success);
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    if (velora::ui::forms::OutlinedButton("Del", ImVec2(48, 32)))
        deleteSelected(*active_);
    ImGui::PopStyleVar();
}

void Lattice::drawPane(LatticePane& p, const char* id, float width)
{
    ImGui::BeginChild(id, ImVec2(width, 0), true);
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0))
        active_ = &p;

    auto list = sortedList(p);

    /* Sort headers */
    if (view_ == LatticeView::Details)
    {
        if (ImGui::BeginTable("##ft", 4,
                              ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable |
                                  ImGuiTableFlags_NoBordersInBody))
        {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 70);
            ImGui::TableSetupColumn("Modified", ImGuiTableColumnFlags_WidthFixed, 120);
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();

            if (ImGui::TableGetSortSpecs())
            {
                auto* specs = ImGui::TableGetSortSpecs();
                if (specs && specs->SpecsCount > 0)
                {
                    auto& s = specs->Specs[0];
                    if (s.ColumnIndex == 0) sort_ = LatticeSort::Name;
                    if (s.ColumnIndex == 1) sort_ = LatticeSort::Size;
                    if (s.ColumnIndex == 2) sort_ = LatticeSort::Type;
                    if (s.ColumnIndex == 3) sort_ = LatticeSort::MTime;
                    sortAsc_ = (s.SortDirection == ImGuiSortDirection_Ascending);
                }
            }

            for (int i = 0; i < (int)list.size(); ++i)
            {
                const auto& n = list[i];
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                bool selected = std::find(p.selected.begin(), p.selected.end(), n.path) != p.selected.end();
                std::string label = (n.isDir ? "DIR  " : "FILE ") + n.name;
                ImGuiSelectableFlags fl = ImGuiSelectableFlags_SpanAllColumns |
                                         ImGuiSelectableFlags_AllowDoubleClick;
                if (ImGui::Selectable(label.c_str(), selected, fl))
                {
                    active_ = &p;
                    if (ImGui::GetIO().KeyCtrl)
                    {
                        if (selected)
                            p.selected.erase(std::remove(p.selected.begin(), p.selected.end(), n.path), p.selected.end());
                        else
                            p.selected.push_back(n.path);
                    }
                    else
                    {
                        p.selected = {n.path};
                        p.focusPath = n.path;
                        refreshPreview(n.path, n.isDir);
                    }
                    if (ImGui::IsMouseDoubleClicked(0))
                        openNode(p, n);
                }
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
                {
                    ImGui::SetDragDropPayload("VELFS_PATH", n.path.c_str(), n.path.size() + 1);
                    ImGui::Text("%s", n.name.c_str());
                    ImGui::TextDisabled("Drop on Facide to create shortcut");
                    ImGui::EndDragDropSource();
                }

                if (ImGui::BeginPopupContextItem())
                {
                    active_ = &p;
                    if (p.selected.empty())
                        p.selected = {n.path};
                    if (ImGui::MenuItem("Open")) openNode(p, n);
                    if (ImGui::MenuItem("Copy"))
                    {
                        clipPath_ = n.path;
                        clipCut_ = false;
                        status_ = "Copied to clipboard";
                    }
                    if (ImGui::MenuItem("Cut"))
                    {
                        clipPath_ = n.path;
                        clipCut_ = true;
                        status_ = "Cut to clipboard";
                    }
                    if (ImGui::MenuItem("Paste", nullptr, false, !clipPath_.empty()))
                        applyClipboard(p);
                    ImGui::Separator();
                    if (ImGui::MenuItem("Rename"))
                    {
                        std::snprintf(renameBuf_, sizeof(renameBuf_), "%s", n.name.c_str());
                        ImGui::OpenPopup("RenameDlg");
                    }
                    if (ImGui::MenuItem("Delete"))
                    {
                        p.selected = {n.path};
                        deleteSelected(p);
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Properties"))
                        refreshPreview(n.path, n.isDir);
                    ImGui::EndPopup();
                }

                /* Rename modal needs to be at window level - handle below */
                ImGui::TableNextColumn();
                if (n.isDir) ImGui::TextDisabled("—");
                else
                {
                    if (n.size < 1024) ImGui::Text("%zu B", n.size);
                    else if (n.size < 1024 * 1024) ImGui::Text("%.1f KB", n.size / 1024.0);
                    else ImGui::Text("%.1f MB", n.size / (1024.0 * 1024.0));
                }
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(n.isDir ? "Folder" : (n.ext.empty() ? "File" : n.ext.c_str()));
                ImGui::TableNextColumn();
                if (n.mtime > 0)
                {
                    std::time_t t = (std::time_t)n.mtime;
                    std::tm tm{};
#if defined(_MSC_VER)
                    localtime_s(&tm, &t);
#else
                    if (auto* pt = std::localtime(&t)) tm = *pt;
#endif
                    ImGui::Text("%02d.%02d.%02d %02d:%02d", tm.tm_mday, tm.tm_mon + 1,
                                (tm.tm_year + 1900) % 100, tm.tm_hour, tm.tm_min);
                }
                else ImGui::TextDisabled("—");
            }
            ImGui::EndTable();
        }
    }
    else
    {
        /* Icon grid */
        float cell = 96.f;
        int cols = std::max(1, (int)(ImGui::GetContentRegionAvail().x / cell));
        int col = 0;
        for (int i = 0; i < (int)list.size(); ++i)
        {
            const auto& n = list[i];
            if (col) ImGui::SameLine();
            ImGui::PushID(i);
            bool selected = std::find(p.selected.begin(), p.selected.end(), n.path) != p.selected.end();
            ImGui::BeginGroup();
            ImVec2 p0 = ImGui::GetCursorScreenPos();
            if (selected)
                ImGui::GetWindowDrawList()->AddRectFilled(p0, ImVec2(p0.x + cell - 8, p0.y + cell - 4),
                                                          IM_COL32(100, 80, 180, 60), 12.f);
            ImGui::Button(n.isDir ? "[DIR]" : "[FILE]", ImVec2(cell - 16, 48));
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
                openNode(p, n);
            if (ImGui::IsItemClicked())
            {
                active_ = &p;
                p.selected = {n.path};
                refreshPreview(n.path, n.isDir);
            }
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                ImGui::SetDragDropPayload("VELFS_PATH", n.path.c_str(), n.path.size() + 1);
                ImGui::Text("%s", n.name.c_str());
                ImGui::EndDragDropSource();
            }
            ImGui::PushTextWrapPos(p0.x + cell - 12);
            ImGui::TextUnformatted(n.name.c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndGroup();
            ImGui::PopID();
            col = (col + 1) % cols;
        }
    }

    /* Empty area context */
    if (ImGui::BeginPopupContextWindow("##paneempty", ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight))
    {
        active_ = &p;
        if (ImGui::MenuItem("Paste", nullptr, false, !clipPath_.empty()))
            applyClipboard(p);
        if (ImGui::MenuItem("New folder"))
        {
            velora::fs::VelFS::get().mkdir(velora::fs::VelFS::join(p.cwd, "NewFolder"));
        }
        if (ImGui::MenuItem("Refresh"))
            status_ = "Refreshed";
        ImGui::EndPopup();
    }

    /* Keyboard */
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
    {
        if (ImGui::IsKeyPressed(ImGuiKey_Backspace)) goUp(p);
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C) && !p.selected.empty())
        {
            clipPath_ = p.selected.front();
            clipCut_ = false;
        }
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_X) && !p.selected.empty())
        {
            clipPath_ = p.selected.front();
            clipCut_ = true;
        }
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V))
            applyClipboard(p);
        if (ImGui::IsKeyPressed(ImGuiKey_Delete) && !p.selected.empty())
            deleteSelected(p);
        if (ImGui::IsKeyPressed(ImGuiKey_F5))
            status_ = "Refreshed";
        if (ImGui::IsKeyPressed(ImGuiKey_Enter) && !p.selected.empty())
        {
            for (const auto& n : list)
                if (n.path == p.selected.front())
                {
                    openNode(p, n);
                    break;
                }
        }
    }

    ImGui::EndChild();
}

void Lattice::drawPreview()
{
    ImGui::BeginChild("##preview", ImVec2(0, 0), true);
    ImGui::TextUnformatted("Information");
    ImGui::Separator();
    if (previewPath_.empty())
        ImGui::TextDisabled("Select an item");
    else
    {
        ImGui::TextWrapped("%s", previewPath_.c_str());
        ImGui::Separator();
        ImGui::BeginChild("##prevbody", ImVec2(0, 0), false);
        ImGui::TextUnformatted(previewText_.c_str());
        ImGui::EndChild();
    }
    ImGui::EndChild();
}

void Lattice::drawStatus()
{
    ImGui::TextDisabled("%s", status_.empty() ? active_->cwd.c_str() : status_.c_str());
    if (!clipPath_.empty())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("· Clipboard: %s (%s)", clipPath_.c_str(), clipCut_ ? "cut" : "copy");
    }
}

void Lattice::requestOpen(const std::string& vpath)
{
    pendingOpen_ = vpath;
}

void Lattice::draw()
{
    if (!pendingOpen_.empty())
    {
        std::string p = pendingOpen_;
        pendingOpen_.clear();
        if (velora::fs::VelFS::get().isDir(p))
            navigate(*active_, p);
        else
        {
            navigate(*active_, velora::fs::VelFS::parent(p));
            active_->selected = {p};
            active_->focusPath = p;
            refreshPreview(p, false);
        }
        status_ = "Opened " + p;
    }

    auto& theme = velora::ui::Theme::get();
    (void)theme;

    drawToolbar();
    ImGui::Separator();

    drawSidebar();
    ImGui::SameLine();

    ImVec2 avail = ImGui::GetContentRegionAvail();
    avail.y -= 28;
    float rest = avail.x;
    float prevW = preview_ ? 220.f : 0.f;
    rest -= prevW;
    if (preview_) rest -= 8.f;

    if (dual_)
    {
        float half = rest * 0.5f - 4.f;
        drawPane(left_, "##left", half);
        ImGui::SameLine();
        drawPane(right_, "##right", half);
    }
    else
    {
        drawPane(left_, "##left", rest);
        active_ = &left_;
    }

    if (preview_)
    {
        ImGui::SameLine();
        ImGui::BeginChild("##prevwrap", ImVec2(prevW, avail.y), false);
        drawPreview();
        ImGui::EndChild();
    }

    drawStatus();
}
} // namespace velora::apps
