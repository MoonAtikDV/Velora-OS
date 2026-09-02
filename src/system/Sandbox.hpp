#pragma once

#include <string>

namespace velora::system
{
/**
 * Minimal browser sandbox for Clor.
 *
 * Goals (simple, pragmatic):
 *  - browser work runs outside the main VeloraOS process
 *  - dedicated cache directory only (no VelFS User/System writes)
 *  - on Windows: Low Integrity Level process when possible
 *  - main OS only issues Navigate(url); no shared memory with kernel
 */
class BrowserSandbox
{
public:
    static BrowserSandbox& get();

    /** Ensure sandbox dirs exist under VelFS/System/Cache/clor-sandbox */
    bool prepare();

    /** Open URL inside the sandbox (separate process). */
    bool navigate(const std::string& url);

    const std::string& cacheDir() const { return cacheDir_; }
    const std::string& lastError() const { return lastError_; }

private:
    std::string cacheDir_;
    std::string lastError_;

    bool navigateWindows(const std::string& url);
    bool navigateFallback(const std::string& url);
};
} // namespace velora::system
