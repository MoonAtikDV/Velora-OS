#include "gfx/Graphics.hpp"
#include "ui/GlCompat.hpp"

#include <iostream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace velora::gfx
{
Graphics& Graphics::get()
{
    static Graphics g;
    return g;
}

const char* Graphics::backendName() const
{
    return info_.backend == Backend::Vulkan ? "Vulkan" : "OpenGL";
}

const char* Graphics::preferredName() const
{
    return preferred_ == Backend::Vulkan ? "Vulkan" : "OpenGL";
}

void Graphics::setPreferred(Backend b)
{
    preferred_ = b;
    /* Active compositor remains OpenGL until a full Vulkan renderer ships.
       Preference is stored and reported for apps / future path. */
    if (b == Backend::Vulkan && info_.vulkanAvailable)
        info_.backend = Backend::Vulkan; /* logical preference */
    else
        info_.backend = Backend::OpenGL;
}

void Graphics::detect()
{
    info_.openglAvailable = true;
    info_.backend = Backend::OpenGL;
    const char* ren = (const char*)glGetString(GL_RENDERER);
    const char* ver = (const char*)glGetString(GL_VERSION);
    info_.renderer = ren ? ren : "unknown";
    info_.version = ver ? ver : "unknown";

#ifdef _WIN32
    HMODULE mod = LoadLibraryA("vulkan-1.dll");
    if (mod)
    {
        info_.vulkanAvailable = true;
        FreeLibrary(mod);
    }
#else
    info_.vulkanAvailable = false;
#endif
    std::cout << "[gfx] backend=" << backendName()
              << " renderer=" << info_.renderer
              << " version=" << info_.version
              << " vulkan=" << (info_.vulkanAvailable ? "yes" : "no") << std::endl;
}
} // namespace velora::gfx
