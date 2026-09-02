#include "apps/ClorEngine.hpp"
#include "system/Sandbox.hpp"
#include "fs/VelFS.hpp"
#include "system/Notifications.hpp"
#include "core/Access.hpp"
#include "apps/EmbeddedBrowser.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <urlmon.h>
#pragma comment(lib, "urlmon")
#endif

namespace velora::apps
{
ClorEngine& ClorEngine::get()
{
    static ClorEngine e;
    return e;
}

void ClorEngine::ensureDefaults()
{
    if (tabs_.empty())
        newTab("https://duckduckgo.com/");
}

ClorTab* ClorEngine::current()
{
    if (tabs_.empty()) return nullptr;
    if (active_ < 0 || active_ >= (int)tabs_.size()) active_ = 0;
    return &tabs_[active_];
}

int ClorEngine::newTab(const std::string& url)
{
    ClorTab t;
    t.id = "t" + std::to_string(tabSeq_++);
    t.url = url;
    t.title = "New tab";
    tabs_.push_back(std::move(t));
    active_ = (int)tabs_.size() - 1;
    navigate(url);
    return active_;
}

void ClorEngine::closeTab(int index)
{
    if (index < 0 || index >= (int)tabs_.size()) return;
    tabs_.erase(tabs_.begin() + index);
    if (tabs_.empty())
        newTab("https://duckduckgo.com/");
    if (active_ >= (int)tabs_.size())
        active_ = (int)tabs_.size() - 1;
}

void ClorEngine::setActive(int index)
{
    if (index >= 0 && index < (int)tabs_.size())
        active_ = index;
}

bool ClorEngine::isSafeUrl(const std::string& url) const
{
    return url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0 || url.rfind("about:", 0) == 0;
}

void ClorEngine::pushHistory(ClorTab& t, const std::string& url)
{
    if (!t.url.empty() && t.url != url)
        t.back.push_back(t.url);
    t.forward.clear();
    t.url = url;
    t.title = url;
    auto p = url.find("://");
    if (p != std::string::npos)
    {
        auto host = url.substr(p + 3);
        auto s = host.find('/');
        t.title = s == std::string::npos ? host : host.substr(0, s);
    }
}

static std::string stripHtml(const std::string& html)
{
    std::string out;
    out.reserve(html.size() / 2);
    bool inTag = false;
    bool inScript = false;
    for (size_t i = 0; i < html.size(); ++i)
    {
        char c = html[i];
        if (c == '<')
        {
            inTag = true;
            if (i + 7 < html.size())
            {
                std::string tag = html.substr(i + 1, 6);
                for (auto& ch : tag) ch = (char)std::tolower((unsigned char)ch);
                if (tag == "script" || tag == "style>")
                    inScript = true;
                if (tag == "/scrip" || tag == "/style")
                    inScript = false;
            }
            continue;
        }
        if (c == '>') { inTag = false; continue; }
        if (inTag || inScript) continue;
        if (c == '&')
        {
            if (html.compare(i, 4, "&lt;") == 0) { out.push_back('<'); i += 3; continue; }
            if (html.compare(i, 4, "&gt;") == 0) { out.push_back('>'); i += 3; continue; }
            if (html.compare(i, 5, "&amp;") == 0) { out.push_back('&'); i += 4; continue; }
            if (html.compare(i, 6, "&nbsp;") == 0) { out.push_back(' '); i += 5; continue; }
        }
        out.push_back(c);
    }
    /* collapse whitespace */
    std::string compact;
    bool space = false;
    for (char c : out)
    {
        if (c == '\r') continue;
        if (c == '\n' || c == '\t') c = ' ';
        if (c == ' ')
        {
            if (space) continue;
            space = true;
        }
        else space = false;
        compact.push_back(c);
    }
    if (compact.size() > 120000)
        compact.resize(120000);
    return compact;
}

bool ClorEngine::fetchPage(ClorTab& t, const std::string& url)
{
    t.pageText.clear();
    t.pageError.clear();
    t.loading = true;

#ifdef _WIN32
    char tmpPath[MAX_PATH];
    char tmpFile[MAX_PATH];
    GetTempPathA(MAX_PATH, tmpPath);
    GetTempFileNameA(tmpPath, "clr", 0, tmpFile);
    HRESULT hr = URLDownloadToFileA(nullptr, url.c_str(), tmpFile, 0, nullptr);
    if (FAILED(hr))
    {
        t.loading = false;
        t.pageError = "Download failed (network or blocked URL)";
        status_ = t.pageError;
        return false;
    }
    std::ifstream in(tmpFile, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    in.close();
    DeleteFileA(tmpFile);
    std::string html = ss.str();
    t.pageText = stripHtml(html);
    if (t.pageText.empty())
        t.pageText = "(Empty page body)";
    /* title from <title> */
    auto tl = html.find("<title");
    if (tl == std::string::npos) tl = html.find("<TITLE");
    if (tl != std::string::npos)
    {
        auto gt = html.find('>', tl);
        auto te = html.find("</title>", gt);
        if (te == std::string::npos) te = html.find("</TITLE>", gt);
        if (gt != std::string::npos && te != std::string::npos && te > gt)
        {
            std::string title = html.substr(gt + 1, te - gt - 1);
            if (!title.empty()) t.title = title;
        }
    }
    t.loading = false;
    status_ = "Loaded " + t.title;
    return true;
#else
    t.loading = false;
    t.pageError = "Fetch not implemented on this platform";
    status_ = t.pageError;
    return false;
#endif
}

bool ClorEngine::navigate(const std::string& url)
{
    ensureDefaults();
    ClorTab* t = current();
    if (!t) return false;

    std::string u = url;
    if (u.find("://") == std::string::npos && u.rfind("about:", 0) != 0)
        u = "https://" + u;

    if (!isSafeUrl(u))
    {
        status_ = "Blocked (only http/https)";
        t->pageError = status_;
        return false;
    }

    if (u.rfind("about:", 0) == 0)
    {
        pushHistory(*t, u);
        t->loading = false;
        t->pageText = "About: Clor browser\nSandboxed fetch + optional external Edge/Chrome.";
        t->pageError.clear();
        status_ = "about page";
        return true;
    }

    if (!velora::core::AccessControl::get().check("clor", velora::core::Perm::Network))
    {
        status_ = "Denied: network permission";
        t->pageError = status_;
        return false;
    }

    pushHistory(*t, u);
    bool embOk = velora::apps::EmbeddedBrowser::get().navigate(u);
    bool ok = embOk || fetchPage(*t, u);

    if (!embOk)
    {
        /* Fallback: external sandbox only if in-OS WebView2 unavailable */
        auto& box = velora::system::BrowserSandbox::get();
        if (box.navigate(u))
            sandboxPath_ = box.cacheDir();
        else if (!ok)
            status_ = "Error: " + box.lastError();
    }
    else
        status_ = "WebView2 · " + u;

    if (ok)
        velora::system::NotificationCenter::get().post("Clor", "Opened " + t->title, "clor",
                                                       velora::system::NotifLevel::Info, 2.5f);
    return ok;
}

bool ClorEngine::openExternal(const std::string& url)
{
    auto& box = velora::system::BrowserSandbox::get();
    bool ok = box.navigate(url);
    status_ = ok ? "External sandbox launched" : box.lastError();
    sandboxPath_ = box.cacheDir();
    return ok;
}

bool ClorEngine::goBack()
{
    ClorTab* t = current();
    if (!t || t->back.empty()) return false;
    t->forward.push_back(t->url);
    std::string u = t->back.back();
    t->back.pop_back();
    t->url = u;
    return fetchPage(*t, u);
}

bool ClorEngine::goForward()
{
    ClorTab* t = current();
    if (!t || t->forward.empty()) return false;
    t->back.push_back(t->url);
    std::string u = t->forward.back();
    t->forward.pop_back();
    t->url = u;
    return fetchPage(*t, u);
}

bool ClorEngine::reload()
{
    ClorTab* t = current();
    if (!t) return false;
    return fetchPage(*t, t->url);
}

void ClorEngine::loadPersistent()
{
    auto raw = velora::fs::VelFS::get().readText("/System/Config/clor_bookmarks.txt");
    bookmarks_.clear();
    std::istringstream in(raw);
    std::string line;
    while (std::getline(in, line))
    {
        auto tab = line.find('\t');
        if (tab == std::string::npos) continue;
        bookmarks_.push_back({line.substr(0, tab), line.substr(tab + 1)});
    }
}

void ClorEngine::savePersistent()
{
    std::ostringstream o;
    for (const auto& b : bookmarks_)
        o << b.title << '\t' << b.url << '\n';
    velora::fs::VelFS::get().writeText("/System/Config/clor_bookmarks.txt", o.str());
}
} // namespace velora::apps
