/*************************************************************************/
/*  android_permissions.h                                                */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2021 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2021 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#ifndef ANDROID_CONSTANTS
#define ANDROID_CONSTANTS

#include "main/splash.gen.h"
#include "platform/android/logo.gen.h"
#include "platform/android/run_icon.gen.h"

static const char *android_perms[] = {
    "ACCESS_CHECKIN_PROPERTIES",
    "ACCESS_COARSE_LOCATION",
    "ACCESS_FINE_LOCATION",
    "ACCESS_LOCATION_EXTRA_COMMANDS",
    "ACCESS_MOCK_LOCATION",
    "ACCESS_NETWORK_STATE",
    "ACCESS_SURFACE_FLINGER",
    "ACCESS_WIFI_STATE",
    "ACCOUNT_MANAGER",
    "ADD_VOICEMAIL",
    "AUTHENTICATE_ACCOUNTS",
    "BATTERY_STATS",
    "BIND_ACCESSIBILITY_SERVICE",
    "BIND_APPWIDGET",
    "BIND_DEVICE_ADMIN",
    "BIND_INPUT_METHOD",
    "BIND_NFC_SERVICE",
    "BIND_NOTIFICATION_LISTENER_SERVICE",
    "BIND_PRINT_SERVICE",
    "BIND_REMOTEVIEWS",
    "BIND_TEXT_SERVICE",
    "BIND_VPN_SERVICE",
    "BIND_WALLPAPER",
    "BLUETOOTH",
    "BLUETOOTH_ADMIN",
    "BLUETOOTH_PRIVILEGED",
    "BRICK",
    "BROADCAST_PACKAGE_REMOVED",
    "BROADCAST_SMS",
    "BROADCAST_STICKY",
    "BROADCAST_WAP_PUSH",
    "CALL_PHONE",
    "CALL_PRIVILEGED",
    "CAMERA",
    "CAPTURE_AUDIO_OUTPUT",
    "CAPTURE_SECURE_VIDEO_OUTPUT",
    "CAPTURE_VIDEO_OUTPUT",
    "CHANGE_COMPONENT_ENABLED_STATE",
    "CHANGE_CONFIGURATION",
    "CHANGE_NETWORK_STATE",
    "CHANGE_WIFI_MULTICAST_STATE",
    "CHANGE_WIFI_STATE",
    "CLEAR_APP_CACHE",
    "CLEAR_APP_USER_DATA",
    "CONTROL_LOCATION_UPDATES",
    "DELETE_CACHE_FILES",
    "DELETE_PACKAGES",
    "DEVICE_POWER",
    "DIAGNOSTIC",
    "DISABLE_KEYGUARD",
    "DUMP",
    "EXPAND_STATUS_BAR",
    "FACTORY_TEST",
    "FLASHLIGHT",
    "FORCE_BACK",
    "GET_ACCOUNTS",
    "GET_PACKAGE_SIZE",
    "GET_TASKS",
    "GET_TOP_ACTIVITY_INFO",
    "GLOBAL_SEARCH",
    "HARDWARE_TEST",
    "INJECT_EVENTS",
    "INSTALL_LOCATION_PROVIDER",
    "INSTALL_PACKAGES",
    "INSTALL_SHORTCUT",
    "INTERNAL_SYSTEM_WINDOW",
    "INTERNET",
    "KILL_BACKGROUND_PROCESSES",
    "LOCATION_HARDWARE",
    "MANAGE_ACCOUNTS",
    "MANAGE_APP_TOKENS",
    "MANAGE_DOCUMENTS",
    "MASTER_CLEAR",
    "MEDIA_CONTENT_CONTROL",
    "MODIFY_AUDIO_SETTINGS",
    "MODIFY_PHONE_STATE",
    "MOUNT_FORMAT_FILESYSTEMS",
    "MOUNT_UNMOUNT_FILESYSTEMS",
    "NFC",
    "PERSISTENT_ACTIVITY",
    "PROCESS_OUTGOING_CALLS",
    "READ_CALENDAR",
    "READ_CALL_LOG",
    "READ_CONTACTS",
    "READ_EXTERNAL_STORAGE",
    "READ_FRAME_BUFFER",
    "READ_HISTORY_BOOKMARKS",
    "READ_INPUT_STATE",
    "READ_LOGS",
    "READ_PHONE_STATE",
    "READ_PROFILE",
    "READ_SMS",
    "READ_SOCIAL_STREAM",
    "READ_SYNC_SETTINGS",
    "READ_SYNC_STATS",
    "READ_USER_DICTIONARY",
    "REBOOT",
    "RECEIVE_BOOT_COMPLETED",
    "RECEIVE_MMS",
    "RECEIVE_SMS",
    "RECEIVE_WAP_PUSH",
    "RECORD_AUDIO",
    "REORDER_TASKS",
    "RESTART_PACKAGES",
    "SEND_RESPOND_VIA_MESSAGE",
    "SEND_SMS",
    "SET_ACTIVITY_WATCHER",
    "SET_ALARM",
    "SET_ALWAYS_FINISH",
    "SET_ANIMATION_SCALE",
    "SET_DEBUG_APP",
    "SET_ORIENTATION",
    "SET_POINTER_SPEED",
    "SET_PREFERRED_APPLICATIONS",
    "SET_PROCESS_LIMIT",
    "SET_TIME",
    "SET_TIME_ZONE",
    "SET_WALLPAPER",
    "SET_WALLPAPER_HINTS",
    "SIGNAL_PERSISTENT_PROCESSES",
    "STATUS_BAR",
    "SUBSCRIBED_FEEDS_READ",
    "SUBSCRIBED_FEEDS_WRITE",
    "SYSTEM_ALERT_WINDOW",
    "TRANSMIT_IR",
    "UNINSTALL_SHORTCUT",
    "UPDATE_DEVICE_STATS",
    "USE_CREDENTIALS",
    "USE_SIP",
    "VIBRATE",
    "WAKE_LOCK",
    "WRITE_APN_SETTINGS",
    "WRITE_CALENDAR",
    "WRITE_CALL_LOG",
    "WRITE_CONTACTS",
    "WRITE_EXTERNAL_STORAGE",
    "WRITE_GSERVICES",
    "WRITE_HISTORY_BOOKMARKS",
    "WRITE_PROFILE",
    "WRITE_SECURE_SETTINGS",
    "WRITE_SETTINGS",
    "WRITE_SMS",
    "WRITE_SOCIAL_STREAM",
    "WRITE_SYNC_SETTINGS",
    "WRITE_USER_DICTIONARY",
    nullptr
};

static const char *SPLASH_IMAGE_EXPORT_PATH = "res/drawable-nodpi/splash.png";
static const char *LEGACY_BUILD_SPLASH_IMAGE_EXPORT_PATH = "res/drawable-nodpi-v4/splash.png";
static const char *SPLASH_BG_COLOR_PATH = "res/drawable-nodpi/splash_bg_color.png";
static const char *LEGACY_BUILD_SPLASH_BG_COLOR_PATH = "res/drawable-nodpi-v4/splash_bg_color.png";
static const char *SPLASH_CONFIG_PATH = "res://android/build/res/drawable/splash_drawable.xml";

const String SPLASH_CONFIG_XML_CONTENT = R"SPLASH(<?xml version="1.0" encoding="utf-8"?>
<layer-list xmlns:android="http://schemas.android.com/apk/res/android">
    <item android:drawable="@drawable/splash_bg_color" />
    <item>
        <bitmap
                android:gravity="center"
                android:filter="%s"
                android:src="@drawable/splash" />
    </item>
</layer-list>
)SPLASH";

struct LauncherIcon {
    const char *export_path;
    int dimensions = 0;
};

static const int icon_densities_count = 6;
static const char *launcher_icon_option = "launcher_icons/main_192x192";
static const char *launcher_adaptive_icon_foreground_option = "launcher_icons/adaptive_foreground_432x432";
static const char *launcher_adaptive_icon_background_option = "launcher_icons/adaptive_background_432x432";

static const LauncherIcon launcher_icons[icon_densities_count] = {
    { "res/mipmap-xxxhdpi-v4/icon.png", 192 },
    { "res/mipmap-xxhdpi-v4/icon.png", 144 },
    { "res/mipmap-xhdpi-v4/icon.png", 96 },
    { "res/mipmap-hdpi-v4/icon.png", 72 },
    { "res/mipmap-mdpi-v4/icon.png", 48 },
    { "res/mipmap/icon.png", 192 }
};

static const LauncherIcon launcher_adaptive_icon_foregrounds[icon_densities_count] = {
    { "res/mipmap-xxxhdpi-v4/icon_foreground.png", 432 },
    { "res/mipmap-xxhdpi-v4/icon_foreground.png", 324 },
    { "res/mipmap-xhdpi-v4/icon_foreground.png", 216 },
    { "res/mipmap-hdpi-v4/icon_foreground.png", 162 },
    { "res/mipmap-mdpi-v4/icon_foreground.png", 108 },
    { "res/mipmap/icon_foreground.png", 432 }
};

static const LauncherIcon launcher_adaptive_icon_backgrounds[icon_densities_count] = {
    { "res/mipmap-xxxhdpi-v4/icon_background.png", 432 },
    { "res/mipmap-xxhdpi-v4/icon_background.png", 324 },
    { "res/mipmap-xhdpi-v4/icon_background.png", 216 },
    { "res/mipmap-hdpi-v4/icon_background.png", 162 },
    { "res/mipmap-mdpi-v4/icon_background.png", 108 },
    { "res/mipmap/icon_background.png", 432 }
};

static const int EXPORT_FORMAT_APK = 0;
static const int EXPORT_FORMAT_AAB = 1;


#endif
