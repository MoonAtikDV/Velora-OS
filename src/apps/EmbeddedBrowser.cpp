#include "apps/EmbeddedBrowser.hpp"
#include <iostream>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#endif

#if defined(_WIN32) && defined(VELORA_HAS_WEBVIEW2)
#include <objbase.h>
#include "WebView2.h"

namespace
{
/* Exact IIDs from WebView2 IDL (do not use __uuidof — broken on MinGW link) */
// ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler
static const IID IID_EnvCompleted = {
    0x4e8a3389, 0xc9d8, 0x4bd2, {0xb6, 0xb5, 0x12, 0x4e, 0xee, 0x6c, 0xc1, 0x4d}};
// ICoreWebView2CreateCoreWebView2ControllerCompletedHandler  
static const IID IID_CtrlCompleted = {
    0x6c4819e3, 0xc24f, 0x4ad1, {0x82, 0x70, 0x5e, 0x4e, 0xee, 0x6c, 0xc1, 0x4e}};

struct HostState
{
    ICoreWebView2Controller* controller = nullptr;
    ICoreWebView2* webview = nullptr;
    bool ready = false;
    std::string* error = nullptr;
    std::string pendingUrl;
    RECT bounds{};
    bool visible = false;
};

class ControllerHandler final : public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler
{
public:
    explicit ControllerHandler(HostState* s) : s_(s) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override
    {
        if (!ppv) return E_POINTER;
        if (InlineIsEqualGUID(riid, IID_IUnknown) || InlineIsEqualGUID(riid, IID_CtrlCompleted))
        {
            *ppv = static_cast<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return (ULONG)InterlockedIncrement(&refs_); }
    ULONG STDMETHODCALLTYPE Release() override
    {
        LONG r = InterlockedDecrement(&refs_);
        if (!r) delete this;
        return (ULONG)r;
    }
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT result, ICoreWebView2Controller* controller) override
    {
        if (FAILED(result) || !controller)
        {
            if (s_->error) *s_->error = "Controller create failed";
            std::cerr << "[clor] controller hr=" << std::hex << result << std::endl;
            return result;
        }
        s_->controller = controller;
        s_->controller->AddRef();
        ICoreWebView2* wv = nullptr;
        s_->controller->get_CoreWebView2(&wv);
        s_->webview = wv;
        s_->ready = true;
        if (s_->error) s_->error->clear();
        s_->controller->put_Bounds(s_->bounds);
        s_->controller->put_IsVisible(s_->visible ? TRUE : FALSE);
        if (!s_->pendingUrl.empty() && wv)
        {
            std::wstring w(s_->pendingUrl.begin(), s_->pendingUrl.end());
            wv->Navigate(w.c_str());
        }
        std::cout << "[clor] WebView2 controller ready\n";
        return S_OK;
    }
private:
    HostState* s_;
    LONG refs_ = 1;
};

class EnvHandler final : public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler
{
public:
    EnvHandler(HostState* s, HWND hwnd) : s_(s), hwnd_(hwnd) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override
    {
        if (!ppv) return E_POINTER;
        if (InlineIsEqualGUID(riid, IID_IUnknown) || InlineIsEqualGUID(riid, IID_EnvCompleted))
        {
            *ppv = static_cast<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return (ULONG)InterlockedIncrement(&refs_); }
    ULONG STDMETHODCALLTYPE Release() override
    {
        LONG r = InterlockedDecrement(&refs_);
        if (!r) delete this;
        return (ULONG)r;
    }
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT result, ICoreWebView2Environment* env) override
    {
        if (FAILED(result) || !env)
        {
            if (s_->error) *s_->error = "Environment create failed — check WebView2Runtime folder";
            std::cerr << "[clor] env hr=" << std::hex << result << std::endl;
            return result;
        }
        return env->CreateCoreWebView2Controller(hwnd_, new ControllerHandler(s_));
    }
private:
    HostState* s_;
    HWND hwnd_;
    LONG refs_ = 1;
};

static HostState g_host;
} // namespace
#endif

namespace velora::apps
{
EmbeddedBrowser& EmbeddedBrowser::get()
{
    static EmbeddedBrowser b;
    return b;
}

bool EmbeddedBrowser::init(GLFWwindow* window)
{
    window_ = window;
#if defined(_WIN32) && defined(VELORA_HAS_WEBVIEW2)
    hwndParent_ = glfwGetWin32Window(window);
    if (!hwndParent_) { error_ = "No HWND"; return false; }
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE && hr != S_FALSE)
    {
        error_ = "COM init failed";
        return false;
    }
    g_host.error = &error_;
    g_host.visible = false;

    std::wstring browserFolder;
    const wchar_t* candidates[] = {
        L"WebView2Runtime",
        L"..\\WebView2Runtime",
        L"Microsoft.WebView2.FixedVersionRuntime",
    };
    for (const wchar_t* c : candidates)
    {
        std::wstring probe = std::wstring(c) + L"\\msedgewebview2.exe";
        if (GetFileAttributesW(probe.c_str()) != INVALID_FILE_ATTRIBUTES)
        {
            browserFolder = c;
            std::wcout << L"[clor] Fixed Runtime: " << c << std::endl;
            break;
        }
    }
    wchar_t ud[MAX_PATH];
    GetTempPathW(MAX_PATH, ud);
    std::wstring userData = std::wstring(ud) + L"VeloraClorWebView2";
    CreateDirectoryW(userData.c_str(), nullptr);

    hr = CreateCoreWebView2EnvironmentWithOptions(
        browserFolder.empty() ? nullptr : browserFolder.c_str(),
        userData.c_str(),
        nullptr,
        new EnvHandler(&g_host, (HWND)hwndParent_));
    if (FAILED(hr))
    {
        error_ = "CreateCoreWebView2EnvironmentWithOptions failed";
        std::cerr << "[clor] " << error_ << " hr=" << std::hex << hr << std::endl;
        return false;
    }
    return true;
#else
    error_ = "Build with -DWEBVIEW2_SDK=...";
    return false;
#endif
}

void EmbeddedBrowser::shutdown()
{
#if defined(_WIN32) && defined(VELORA_HAS_WEBVIEW2)
    if (g_host.controller)
    {
        g_host.controller->Close();
        g_host.controller->Release();
        g_host.controller = nullptr;
    }
    g_host.webview = nullptr;
    g_host.ready = false;
#endif
    ready_ = false;
}

void EmbeddedBrowser::setVisible(bool v)
{
    visible_ = v;
#if defined(_WIN32) && defined(VELORA_HAS_WEBVIEW2)
    g_host.visible = v;
    if (g_host.controller)
        g_host.controller->put_IsVisible(v ? TRUE : FALSE);
#endif
}

void EmbeddedBrowser::setBounds(int x, int y, int w, int h)
{
    bx_ = x; by_ = y; bw_ = w; bh_ = h;
#if defined(_WIN32) && defined(VELORA_HAS_WEBVIEW2)
    g_host.bounds = RECT{ x, y, x + w, y + h };
    if (g_host.controller && w > 0 && h > 0)
        g_host.controller->put_Bounds(g_host.bounds);
#endif
}

bool EmbeddedBrowser::navigate(const std::string& url)
{
    url_ = url;
#if defined(_WIN32) && defined(VELORA_HAS_WEBVIEW2)
    g_host.pendingUrl = url;
    ready_ = g_host.ready;
    if (!g_host.webview)
    {
        error_ = g_host.ready ? "No webview" : "WebView2 still starting…";
        return false;
    }
    std::wstring w(url.begin(), url.end());
    HRESULT hr = g_host.webview->Navigate(w.c_str());
    if (FAILED(hr)) { error_ = "Navigate failed"; return false; }
    error_.clear();
    return true;
#else
    return false;
#endif
}

bool EmbeddedBrowser::goBack()
{
#if defined(_WIN32) && defined(VELORA_HAS_WEBVIEW2)
    if (!g_host.webview) return false;
    BOOL can = FALSE;
    g_host.webview->get_CanGoBack(&can);
    if (can) g_host.webview->GoBack();
    return !!can;
#else
    return false;
#endif
}

bool EmbeddedBrowser::goForward()
{
#if defined(_WIN32) && defined(VELORA_HAS_WEBVIEW2)
    if (!g_host.webview) return false;
    BOOL can = FALSE;
    g_host.webview->get_CanGoForward(&can);
    if (can) g_host.webview->GoForward();
    return !!can;
#else
    return false;
#endif
}

bool EmbeddedBrowser::reload()
{
#if defined(_WIN32) && defined(VELORA_HAS_WEBVIEW2)
    if (!g_host.webview) return false;
    g_host.webview->Reload();
    return true;
#else
    return false;
#endif
}

std::string EmbeddedBrowser::currentUrl() const { return url_; }

void EmbeddedBrowser::tick()
{
#if defined(_WIN32) && defined(VELORA_HAS_WEBVIEW2)
    ready_ = g_host.ready;
#endif
}
} // namespace velora::apps
