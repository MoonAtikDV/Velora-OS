#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace velora::system
{
enum class NotifLevel { Info, Success, Warning, Error };

struct Notification
{
    std::uint64_t id = 0;
    std::string title;
    std::string body;
    std::string source; /* app id or "system" */
    NotifLevel level = NotifLevel::Info;
    float age = 0.f;       /* seconds since post */
    float ttl = 6.f;       /* toast visibility; 0 = sticky toast off but stays in center */
    bool read = false;
    bool dismissed = false;
};

class NotificationCenter
{
public:
    static NotificationCenter& get();

    std::uint64_t post(const std::string& title,
                       const std::string& body,
                       const std::string& source = "system",
                       NotifLevel level = NotifLevel::Info,
                       float toastTtl = 5.5f);

    void dismiss(std::uint64_t id);
    void dismissAll();
    void markAllRead();
    void markRead(std::uint64_t id);

    void tick(float dt);

    const std::vector<Notification>& items() const { return items_; }
    int unreadCount() const;

    bool panelOpen = false;
    void togglePanel() { panelOpen = !panelOpen; }

    /* Active toasts = not dismissed, age < ttl, ttl > 0 */
    std::vector<const Notification*> activeToasts() const;

private:
    std::vector<Notification> items_;
    std::uint64_t nextId_ = 1;
    static constexpr size_t kMax = 80;
};
} // namespace velora::system
