#pragma once
#include "imgui.h"
#include <string>
#include <unordered_map>

struct GLFWwindow;

namespace velora::ui
{
enum class CursorKind
{
    Pointer,
    Beam,
    Hand,      /* link */
    Move,
    Horz,
    Vert,
    Dgn1,
    Dgn2,
    Precision,
    Help,
    Unavailable,
    Pin,
    Person,
    Alternate,
    Handwriting
};

class Cursor
{
public:
    static Cursor& get();
    void load(const std::string& mediaRoot);
    void set(CursorKind k);
    void draw(GLFWwindow* window); /* custom overlay; hides OS cursor */
    CursorKind current() const { return kind_; }

private:
    struct Tex { unsigned id = 0; int w = 0, h = 0; int hotspotX = 0, hotspotY = 0; };
    std::unordered_map<int, Tex> tex_;
    CursorKind kind_ = CursorKind::Pointer;
    bool loaded_ = false;
};
} // namespace velora::ui
