#pragma once
#include <string>
#include <cstdint>
struct GLFWwindow;
namespace velora::apps
{
class EmbeddedBrowser
{
public:
    static EmbeddedBrowser& get();
    bool init(GLFWwindow* window);
    void shutdown();
    bool available() const { return ready_; }
    const std::string& lastError() const { return error_; }
    void setVisible(bool v);
    void setBounds(int x, int y, int w, int h);
    bool navigate(const std::string& url);
    bool goBack();
    bool goForward();
    bool reload();
    std::string currentUrl() const;
    void tick();
private:
    bool ready_ = false;
    bool visible_ = false;
    std::string error_;
    std::string url_;
    GLFWwindow* window_ = nullptr;
    void* hwndParent_ = nullptr;
    void* controller_ = nullptr;
    void* webview_ = nullptr;
    int bx_ = 0, by_ = 0, bw_ = 0, bh_ = 0;
};
}
