#pragma once

#include <QString>

// Network access for NTP. Desktop has no system prompt, so the app asks.
// Android grants INTERNET at install; if that is later turned off, the app
// opens the system settings screen.
class NetworkPermission {
public:
    enum class Status { Granted, Denied, Undetermined, Blocked };

    static Status status();
    static void remember(bool allowed);
    static bool canOpenSystemSettings();
    static bool openSystemSettings();
    static QString askText();
    static QString blockedText();
};
