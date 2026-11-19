#pragma once

#include <QString>

#include <functional>

//================================================================================
// Camera access for photographing the other screen. Android shows the system
// permission dialog. Desktop has no Qt permission API, so the app asks.
class CameraPermission {
public:
    enum class Status { Granted, Denied, Undetermined, Blocked };

//================================================================================
    static Status status();
//================================================================================
    static void request(const std::function<void(Status)>& done);
//================================================================================
    static void remember(bool allowed);
//================================================================================
    static bool canOpenSystemSettings();
//================================================================================
    static bool openSystemSettings();
//================================================================================
    static QString askText();
//================================================================================
    static QString blockedText();
};
