#pragma once

namespace velora::system
{
/** Persistent-ish UI preferences (session also saves some fields) */
struct SettingsStore
{
    /* System */
    bool animations = true;
    bool blurDock = true;
    bool reduceMotion = false;
    float uiScale = 1.f;
    int language = 0; /* 0 EN 1 RU */

    /* Personalization */
    bool accentFromWallpaper = true;
    float iconSizeScale = 1.f;
    bool showDesktopLabels = true;
    bool solidColorFallback = false;

    /* Display */
    bool vsync = true;
    int graphicsBackend = 0; /* 0=OpenGL 1=Vulkan (pref) */
    int fpsLimit = 60;
    bool fullscreen = true;
    float gamma = 1.f;

    /* Sound */
    float masterVolume = 0.8f;
    float notifVolume = 0.7f;
    bool mute = false;
    bool soundOnNotif = true;

    /* Network */
    bool onlineServices = true;
    bool iconCacheOnline = true;
    char proxy[128] = "";

    /* Privacy */
    bool telemetry = false;
    bool storeRecent = true;
    bool clearRecentOnExit = false;

    /* Power */
    int sleepMinutes = 30;
    bool confirmShutdown = true;

    /* Notifications */
    bool notifEnabled = true;
    bool notifToasts = true;
    bool notifSound = true;
    int notifToastSec = 5;

    /* Taskbar */
    bool taskbarLabels = false;
    bool taskbarCenter = false;
    bool taskbarAutohide = false;
    float taskbarOpacity = 0.38f;

    /* Facide */
    bool snapToGrid = true;
    bool showGrid = false;
    int gridCols = 25;
    int gridRows = 10;

    /* Accessibility */
    bool highContrast = false;
    bool largeText = false;
    float cursorSize = 1.f;

    /* Storage */
    bool autoCleanCache = false;
    int cacheMaxMb = 512;

    /* Developer */
    bool showFps = false;
    bool debugBounds = false;
    bool verboseLog = false;

    static SettingsStore& get()
    {
        static SettingsStore s;
        return s;
    }
};
} // namespace velora::system
