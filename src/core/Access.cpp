#include "core/Access.hpp"
#include "core/Kernel.hpp"

namespace velora::core
{
AccessControl& AccessControl::get()
{
    static AccessControl a;
    return a;
}

void AccessControl::grant(const std::string& appId, Perm p)
{
    auto it = grants_.find(appId);
    if (it == grants_.end())
        grants_[appId] = p;
    else
        it->second = it->second | p;
}

void AccessControl::revoke(const std::string& appId, Perm p)
{
    auto it = grants_.find(appId);
    if (it == grants_.end()) return;
    it->second = static_cast<Perm>(static_cast<std::uint32_t>(it->second) &
                                   ~static_cast<std::uint32_t>(p));
}

void AccessControl::setMask(const std::string& appId, Perm mask)
{
    grants_[appId] = mask;
}

Perm AccessControl::mask(const std::string& appId) const
{
    auto it = grants_.find(appId);
    if (it != grants_.end()) return it->second;
    return defaultPermsFor(appId, false);
}

bool AccessControl::check(const std::string& appId, Perm need) const
{
    return hasPerm(mask(appId), need);
}

bool AccessControl::checkPid(int pid, Perm need) const
{
    /* Kernel holds process list */
    return true; /* resolved in Kernel::check */
}

std::vector<std::string> AccessControl::listKnownApps() const
{
    std::vector<std::string> out;
    for (const auto& kv : grants_)
        out.push_back(kv.first);
    return out;
}
} // namespace velora::core
