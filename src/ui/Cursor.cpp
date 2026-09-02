#include "ui/Cursor.hpp"
#include "ui/GlCompat.hpp"
#include "third_party/stb_image.h"

#include <GLFW/glfw3.h>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace velora::ui
{
Cursor& Cursor::get()
{
    static Cursor c;
    return c;
}

static unsigned upload(const unsigned char* data, int w, int h)
{
    unsigned tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    return tex;
}

void Cursor::load(const std::string& mediaRoot)
{
    if (loaded_) return;
    auto tryLoad = [&](CursorKind k, const char* name, int hx, int hy) {
        fs::path p = fs::path(mediaRoot) / "cursor" / name;
        if (!fs::exists(p))
            p = fs::path("media/cursor") / name;
        if (!fs::exists(p)) return;
        int w = 0, h = 0, c = 0;
        stbi_set_flip_vertically_on_load(0);
        unsigned char* data = stbi_load(p.string().c_str(), &w, &h, &c, 4);
        if (!data) return;
        Tex t;
        t.id = upload(data, w, h);
        t.w = w; t.h = h;
        t.hotspotX = hx; t.hotspotY = hy;
        tex_[(int)k] = t;
        stbi_image_free(data);
        std::cout << "[cursor] " << name << " " << w << "x" << h << std::endl;
    };
    tryLoad(CursorKind::Pointer, "Pointer.png", 0, 0);
    tryLoad(CursorKind::Beam, "beam.png", 8, 12);
    tryLoad(CursorKind::Hand, "link.png", 6, 0);
    tryLoad(CursorKind::Move, "move.png", 12, 12);
    tryLoad(CursorKind::Horz, "horz.png", 12, 12);
    tryLoad(CursorKind::Vert, "vert.png", 12, 12);
    tryLoad(CursorKind::Dgn1, "dgn1.png", 12, 12);
    tryLoad(CursorKind::Dgn2, "dgn2.png", 12, 12);
    tryLoad(CursorKind::Precision, "precision.png", 12, 12);
    tryLoad(CursorKind::Help, "help.png", 0, 0);
    tryLoad(CursorKind::Unavailable, "unavailable.png", 12, 12);
    tryLoad(CursorKind::Pin, "pin.png", 8, 0);
    tryLoad(CursorKind::Person, "person.png", 8, 0);
    tryLoad(CursorKind::Alternate, "alternate.png", 0, 0);
    tryLoad(CursorKind::Handwriting, "handwriting.png", 4, 20);
    loaded_ = true;
}

void Cursor::set(CursorKind k) { kind_ = k; }

void Cursor::draw(GLFWwindow* window)
{
    if (!window) return;
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    auto it = tex_.find((int)kind_);
    if (it == tex_.end() || !it->second.id)
        it = tex_.find((int)CursorKind::Pointer);
    if (it == tex_.end() || !it->second.id) return;

    ImGuiIO& io = ImGui::GetIO();
    float x = io.MousePos.x - (float)it->second.hotspotX;
    float y = io.MousePos.y - (float)it->second.hotspotY;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    dl->AddImage((ImTextureID)(intptr_t)it->second.id,
                 ImVec2(x, y),
                 ImVec2(x + (float)it->second.w, y + (float)it->second.h));
}
} // namespace velora::ui
