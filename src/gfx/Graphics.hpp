#pragma once
#include <string>

namespace velora::gfx
{
enum class Backend { OpenGL, Vulkan };

struct GraphicsInfo
{
    Backend backend = Backend::OpenGL;
    std::string renderer;
    std::string version;
    bool vulkanAvailable = false;
    bool openglAvailable = true;
};

class Graphics
{
public:
    static Graphics& get();
    void detect();
    void setPreferred(Backend b);
    Backend preferred() const { return preferred_; }
    const GraphicsInfo& info() const { return info_; }
    const char* backendName() const;
    const char* preferredName() const;
private:
    GraphicsInfo info_;
    Backend preferred_ = Backend::OpenGL;
};
} // namespace velora::gfx
