#pragma once

#include "imgui.h"
#include <algorithm>
#include <cmath>

namespace velora::ui::anim
{
inline float clamp01(float t) { return t < 0.f ? 0.f : (t > 1.f ? 1.f : t); }

/* Easing — Material motion inspired */
inline float linear(float t) { return clamp01(t); }
inline float easeOutCubic(float t)
{
    t = clamp01(t);
    float u = 1.f - t;
    return 1.f - u * u * u;
}
inline float easeInOutCubic(float t)
{
    t = clamp01(t);
    return t < 0.5f ? 4.f * t * t * t : 1.f - std::pow(-2.f * t + 2.f, 3.f) / 2.f;
}
inline float easeOutBack(float t)
{
    t = clamp01(t);
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.f;
    float u = t - 1.f;
    return 1.f + c3 * u * u * u + c1 * u * u;
}
inline float easeOutElastic(float t)
{
    t = clamp01(t);
    if (t == 0.f || t == 1.f) return t;
    const float c4 = (2.f * 3.14159265f) / 3.f;
    return std::pow(2.f, -10.f * t) * std::sin((t * 10.f - 0.75f) * c4) + 1.f;
}
inline float easeOutQuint(float t)
{
    t = clamp01(t);
    float u = 1.f - t;
    return 1.f - u * u * u * u * u;
}

/** Exponential smoothing toward target (framerate independent) */
inline float damp(float current, float target, float lambda, float dt)
{
    return target + (current - target) * std::exp(-lambda * dt);
}

inline ImVec2 damp2(ImVec2 c, ImVec2 t, float lambda, float dt)
{
    return ImVec2(damp(c.x, t.x, lambda, dt), damp(c.y, t.y, lambda, dt));
}

inline float lerp(float a, float b, float t) { return a + (b - a) * clamp01(t); }
inline ImVec4 lerp4(ImVec4 a, ImVec4 b, float t)
{
    t = clamp01(t);
    return ImVec4(lerp(a.x, b.x, t), lerp(a.y, b.y, t), lerp(a.z, b.z, t), lerp(a.w, b.w, t));
}

struct Spring
{
    float value = 0.f;
    float vel = 0.f;
    float target = 0.f;
    float stiffness = 180.f;
    float damping = 18.f;

    void setTarget(float t) { target = t; }
    void snap(float v)
    {
        value = target = v;
        vel = 0.f;
    }
    float tick(float dt)
    {
        /* semi-implicit spring */
        float force = (target - value) * stiffness - vel * damping;
        vel += force * dt;
        value += vel * dt;
        if (std::fabs(target - value) < 0.0005f && std::fabs(vel) < 0.01f)
        {
            value = target;
            vel = 0.f;
        }
        return value;
    }
};
} // namespace velora::ui::anim
