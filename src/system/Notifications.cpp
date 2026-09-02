#include "system/Notifications.hpp"
#include <algorithm>

namespace velora::system
{
NotificationCenter& NotificationCenter::get()
{
    static NotificationCenter c;
    return c;
}

std::uint64_t NotificationCenter::post(const std::string& title,
                                       const std::string& body,
                                       const std::string& source,
                                       NotifLevel level,
                                       float toastTtl)
{
    Notification n;
    n.id = nextId_++;
    n.title = title;
    n.body = body;
    n.source = source;
    n.level = level;
    n.ttl = toastTtl;
    n.age = 0.f;
    items_.insert(items_.begin(), std::move(n));
    if (items_.size() > kMax)
        items_.resize(kMax);
    return items_.front().id;
}

void NotificationCenter::dismiss(std::uint64_t id)
{
    items_.erase(std::remove_if(items_.begin(), items_.end(),
                                [&](const Notification& n) { return n.id == id; }),
                 items_.end());
}

void NotificationCenter::dismissAll()
{
    items_.clear();
}

void NotificationCenter::markRead(std::uint64_t id)
{
    for (auto& n : items_)
        if (n.id == id) { n.read = true; return; }
}

void NotificationCenter::markAllRead()
{
    for (auto& n : items_)
        n.read = true;
}

void NotificationCenter::tick(float dt)
{
    for (auto& n : items_)
        n.age += dt;
}

int NotificationCenter::unreadCount() const
{
    int c = 0;
    for (const auto& n : items_)
        if (!n.read && !n.dismissed) ++c;
    return c;
}

std::vector<const Notification*> NotificationCenter::activeToasts() const
{
    std::vector<const Notification*> out;
    for (const auto& n : items_)
    {
        if (n.dismissed) continue;
        if (n.ttl <= 0.f) continue;
        if (n.age < n.ttl)
            out.push_back(&n);
    }
    return out;
}
} // namespace velora::system
