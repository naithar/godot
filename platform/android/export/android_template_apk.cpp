/*************************************************************************/
/*  android_template_apk.cpp                                             */
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

#include "android_template_apk.h"

#include "core/config/project_settings.h"
#include "core/io/image_loader.h"
#include "core/io/marshalls.h"
#include "core/io/zip_io.h"
#include "core/os/dir_access.h"
#include "core/os/file_access.h"
#include "core/os/os.h"
#include "core/templates/safe_refcount.h"
#include "core/version.h"
#include "drivers/png/png_driver_common.h"
#include "editor/editor_export.h"
#include "editor/editor_log.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"

#include "platform/android/export/gradle_export_util.h"
#include "platform/android/plugin/godot_plugin_config.h"

#include <string.h>

void EditorExportPlatformAndroidAPK::get_preset_features(const Ref<EditorExportPreset> &p_preset, List<String> *r_features) {
    String driver = ProjectSettings::get_singleton()->get("rendering/driver/driver_name");
    if (driver == "GLES2") {
        r_features->push_back("etc");
    }
    // FIXME: Review what texture formats are used for Vulkan.
    if (driver == "Vulkan") {
        r_features->push_back("etc2");
    }

    Vector<String> abis = get_enabled_abis(p_preset);
    for (int i = 0; i < abis.size(); ++i) {
        r_features->push_back(abis[i]);
    }
}

void EditorExportPlatformAndroidAPK::get_export_options(List<ExportOption> *r_options) {
    r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "custom_template/debug", PROPERTY_HINT_GLOBAL_FILE, "*.apk"), ""));
    r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "custom_template/release", PROPERTY_HINT_GLOBAL_FILE, "*.apk"), ""));
    r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "custom_template/use_custom_build"), false));
    r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "custom_template/export_format", PROPERTY_HINT_ENUM, "Export APK,Export AAB"), EXPORT_FORMAT_APK));

    Vector<PluginConfigAndroid> plugins_configs = get_plugins();
    for (int i = 0; i < plugins_configs.size(); i++) {
        print_verbose("Found Android plugin " + plugins_configs[i].name);
        r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "plugins/" + plugins_configs[i].name), false));
    }
    plugins_changed.clear();

    Vector<String> abis = get_abis();
    for (int i = 0; i < abis.size(); ++i) {
        String abi = abis[i];
        bool is_default = (abi == "armeabi-v7a" || abi == "arm64-v8a");
        r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "architectures/" + abi), is_default));
    }

    r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "keystore/debug", PROPERTY_HINT_GLOBAL_FILE, "*.keystore,*.jks"), ""));
    r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "keystore/debug_user"), ""));
    r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "keystore/debug_password"), ""));
    r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "keystore/release", PROPERTY_HINT_GLOBAL_FILE, "*.keystore,*.jks"), ""));
    r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "keystore/release_user"), ""));
    r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "keystore/release_password"), ""));

    r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "one_click_deploy/clear_previous_install"), false));

    r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "version/code", PROPERTY_HINT_RANGE, "1,4096,1,or_greater"), 1));
    r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "version/name"), "1.0"));
    r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "package/unique_name", PROPERTY_HINT_PLACEHOLDER_TEXT, "ext.domain.name"), "org.godotengine.$genname"));
    r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "package/name", PROPERTY_HINT_PLACEHOLDER_TEXT, "Game Name [default if blank]"), ""));
    r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "package/signed"), true));

    r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, launcher_icon_option, PROPERTY_HINT_FILE, "*.png"), ""));
    r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, launcher_adaptive_icon_foreground_option, PROPERTY_HINT_FILE, "*.png"), ""));
    r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, launcher_adaptive_icon_background_option, PROPERTY_HINT_FILE, "*.png"), ""));

    r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "graphics/32_bits_framebuffer"), true));
    r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "graphics/opengl_debug"), false));

    r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "xr_features/xr_mode", PROPERTY_HINT_ENUM, "Regular,Oculus Mobile VR"), 0));
    r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "xr_features/hand_tracking", PROPERTY_HINT_ENUM, "None,Optional,Required"), 0));

    r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "screen/immersive_mode"), true));
    r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "screen/support_small"), true));
    r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "screen/support_normal"), true));
    r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "screen/support_large"), true));
    r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "screen/support_xlarge"), true));

    r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "user_data_backup/allow"), false));

    r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "command_line/extra_args"), ""));

    r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "apk_expansion/enable"), false));
    r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "apk_expansion/SALT"), ""));
    r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "apk_expansion/public_key", PROPERTY_HINT_MULTILINE_TEXT), ""));

    r_options->push_back(ExportOption(PropertyInfo(Variant::PACKED_STRING_ARRAY, "permissions/custom_permissions"), PackedStringArray()));

    const char **perms = android_perms;
    while (*perms) {
        r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "permissions/" + String(*perms).to_lower()), false));
        perms++;
    }
}

String EditorExportPlatformAndroidAPK::get_name() const {
    return "Android APK Exporter";
}

String EditorExportPlatformAndroidAPK::get_os_name() const {
    return "Android";
}

Ref<Texture2D> EditorExportPlatformAndroidAPK::get_logo() const {
    return logo;
}

bool EditorExportPlatformAndroidAPK::should_update_export_options() {
    bool export_options_changed = plugins_changed.is_set();
    if (export_options_changed) {
        // don't clear unless we're reporting true, to avoid race
        plugins_changed.clear();
    }
    return export_options_changed;
}

bool EditorExportPlatformAndroidAPK::poll_export() {
    bool dc = devices_changed.is_set();
    if (dc) {
        // don't clear unless we're reporting true, to avoid race
        devices_changed.clear();
    }
    return dc;
}

int EditorExportPlatformAndroidAPK::get_options_count() const {
    MutexLock lock(device_lock);
    return devices.size();
}

String EditorExportPlatformAndroidAPK::get_options_tooltip() const {
    return TTR("Select device from the list");
}

String EditorExportPlatformAndroidAPK::get_option_label(int p_index) const {
    ERR_FAIL_INDEX_V(p_index, devices.size(), "");
    MutexLock lock(device_lock);
    return devices[p_index].name;
}

String EditorExportPlatformAndroidAPK::get_option_tooltip(int p_index) const {
    ERR_FAIL_INDEX_V(p_index, devices.size(), "");
    MutexLock lock(device_lock);
    String s = devices[p_index].description;
    if (devices.size() == 1) {
        // Tooltip will be:
        // Name
        // Description
        s = devices[p_index].name + "\n\n" + s;
    }
    return s;
}

Error EditorExportPlatformAndroidAPK::run(const Ref<EditorExportPreset> &p_preset, int p_device, int p_debug_flags) {
    ERR_FAIL_INDEX_V(p_device, devices.size(), ERR_INVALID_PARAMETER);

    String can_export_error;
    bool can_export_missing_templates;
    if (!can_export(p_preset, can_export_error, can_export_missing_templates)) {
        EditorNode::add_io_error(can_export_error);
        return ERR_UNCONFIGURED;
    }

    MutexLock lock(device_lock);

    EditorProgress ep("run", "Running on " + devices[p_device].name, 3);

    String adb = get_adb_path();

    // Export_temp APK.
    if (ep.step("Exporting APK...", 0)) {
        return ERR_SKIP;
    }

    const bool use_remote = (p_debug_flags & DEBUG_FLAG_REMOTE_DEBUG) || (p_debug_flags & DEBUG_FLAG_DUMB_CLIENT);
    const bool use_reverse = devices[p_device].api_level >= 21;

    if (use_reverse) {
        p_debug_flags |= DEBUG_FLAG_REMOTE_DEBUG_LOCALHOST;
    }

    String tmp_export_path = EditorPaths::get_singleton()->get_cache_dir().plus_file("tmpexport." + uitos(OS::get_singleton()->get_unix_time()) + ".apk");

#define CLEANUP_AND_RETURN(m_err)                         \
{                                                     \
    DirAccess::remove_file_or_error(tmp_export_path); \
    return m_err;                                     \
}

    // Export to temporary APK before sending to device.
    Error err = export_project_helper(p_preset, true, tmp_export_path, EXPORT_FORMAT_APK, true, p_debug_flags);

    if (err != OK) {
        CLEANUP_AND_RETURN(err);
    }

    List<String> args;
    int rv;
    String output;

    bool remove_prev = p_preset->get("one_click_deploy/clear_previous_install");
    String version_name = p_preset->get("version/name");
    String package_name = p_preset->get("package/unique_name");

    if (remove_prev) {
        if (ep.step("Uninstalling...", 1)) {
            CLEANUP_AND_RETURN(ERR_SKIP);
        }

        print_line("Uninstalling previous version: " + devices[p_device].name);

        args.push_back("-s");
        args.push_back(devices[p_device].id);
        args.push_back("uninstall");
        args.push_back(get_package_name(package_name));

        output.clear();
        err = OS::get_singleton()->execute(adb, args, &output, &rv, true);
        print_verbose(output);
    }

    print_line("Installing to device (please wait...): " + devices[p_device].name);
    if (ep.step("Installing to device, please wait...", 2)) {
        CLEANUP_AND_RETURN(ERR_SKIP);
    }

    args.clear();
    args.push_back("-s");
    args.push_back(devices[p_device].id);
    args.push_back("install");
    args.push_back("-r");
    args.push_back(tmp_export_path);

    output.clear();
    err = OS::get_singleton()->execute(adb, args, &output, &rv, true);
    print_verbose(output);
    if (err || rv != 0) {
        EditorNode::add_io_error("Could not install to device.");
        CLEANUP_AND_RETURN(ERR_CANT_CREATE);
    }

    if (use_remote) {
        if (use_reverse) {
            static const char *const msg = "--- Device API >= 21; debugging over USB ---";
            EditorNode::get_singleton()->get_log()->add_message(msg, EditorLog::MSG_TYPE_EDITOR);
            print_line(String(msg).to_upper());

            args.clear();
            args.push_back("-s");
            args.push_back(devices[p_device].id);
            args.push_back("reverse");
            args.push_back("--remove-all");
            output.clear();
            OS::get_singleton()->execute(adb, args, &output, &rv, true);
            print_verbose(output);

            if (p_debug_flags & DEBUG_FLAG_REMOTE_DEBUG) {
                int dbg_port = EditorSettings::get_singleton()->get("network/debug/remote_port");
                args.clear();
                args.push_back("-s");
                args.push_back(devices[p_device].id);
                args.push_back("reverse");
                args.push_back("tcp:" + itos(dbg_port));
                args.push_back("tcp:" + itos(dbg_port));

                output.clear();
                OS::get_singleton()->execute(adb, args, &output, &rv, true);
                print_verbose(output);
                print_line("Reverse result: " + itos(rv));
            }

            if (p_debug_flags & DEBUG_FLAG_DUMB_CLIENT) {
                int fs_port = EditorSettings::get_singleton()->get("filesystem/file_server/port");

                args.clear();
                args.push_back("-s");
                args.push_back(devices[p_device].id);
                args.push_back("reverse");
                args.push_back("tcp:" + itos(fs_port));
                args.push_back("tcp:" + itos(fs_port));

                output.clear();
                err = OS::get_singleton()->execute(adb, args, &output, &rv, true);
                print_verbose(output);
                print_line("Reverse result2: " + itos(rv));
            }
        } else {
            static const char *const msg = "--- Device API < 21; debugging over Wi-Fi ---";
            EditorNode::get_singleton()->get_log()->add_message(msg, EditorLog::MSG_TYPE_EDITOR);
            print_line(String(msg).to_upper());
        }
    }

    if (ep.step("Running on device...", 3)) {
        CLEANUP_AND_RETURN(ERR_SKIP);
    }
    args.clear();
    args.push_back("-s");
    args.push_back(devices[p_device].id);
    args.push_back("shell");
    args.push_back("am");
    args.push_back("start");
    if ((bool)EditorSettings::get_singleton()->get("export/android/force_system_user") && devices[p_device].api_level >= 17) { // Multi-user introduced in Android 17
        args.push_back("--user");
        args.push_back("0");
    }
    args.push_back("-a");
    args.push_back("android.intent.action.MAIN");
    args.push_back("-n");
    args.push_back(get_package_name(package_name) + "/com.godot.game.GodotApp");

    output.clear();
    err = OS::get_singleton()->execute(adb, args, &output, &rv, true);
    print_verbose(output);
    if (err || rv != 0) {
        EditorNode::add_io_error("Could not execute on device.");
        CLEANUP_AND_RETURN(ERR_CANT_CREATE);
    }

    CLEANUP_AND_RETURN(OK);
#undef CLEANUP_AND_RETURN
}

Ref<Texture2D> EditorExportPlatformAndroidAPK::get_run_icon() const {
    return run_icon;
}

String EditorExportPlatformAndroidAPK::get_adb_path() {
    String exe_ext = "";
    if (OS::get_singleton()->get_name() == "Windows") {
        exe_ext = ".exe";
    }
    String sdk_path = EditorSettings::get_singleton()->get("export/android/android_sdk_path");
    return sdk_path.plus_file("platform-tools/adb" + exe_ext);
}

String EditorExportPlatformAndroidAPK::get_apksigner_path() {
    String exe_ext = "";
    if (OS::get_singleton()->get_name() == "Windows") {
        exe_ext = ".bat";
    }
    String apksigner_command_name = "apksigner" + exe_ext;
    String sdk_path = EditorSettings::get_singleton()->get("export/android/android_sdk_path");
    String apksigner_path = "";

    Error errn;
    String build_tools_dir = sdk_path.plus_file("build-tools");
    DirAccessRef da = DirAccess::open(build_tools_dir, &errn);
    if (errn != OK) {
        print_error("Unable to open Android 'build-tools' directory.");
        return apksigner_path;
    }

    // There are additional versions directories we need to go through.
    da->list_dir_begin();
    String sub_dir = da->get_next();
    while (!sub_dir.is_empty()) {
        if (!sub_dir.begins_with(".") && da->current_is_dir()) {
            // Check if the tool is here.
            String tool_path = build_tools_dir.plus_file(sub_dir).plus_file(apksigner_command_name);
            if (FileAccess::exists(tool_path)) {
                apksigner_path = tool_path;
                break;
            }
        }
        sub_dir = da->get_next();
    }
    da->list_dir_end();

    if (apksigner_path.is_empty()) {
        EditorNode::get_singleton()->show_warning(TTR("Unable to find the 'apksigner' tool."));
    }

    return apksigner_path;
}

bool EditorExportPlatformAndroidAPK::can_export(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates) const {
    String err;
    bool valid = false;

    // Look for export templates (first official, and if defined custom templates).

    if (!bool(p_preset->get("custom_template/use_custom_build"))) {
        String template_err;
        bool dvalid = false;
        bool rvalid = false;
        bool has_export_templates = false;

        if (p_preset->get("custom_template/debug") != "") {
            dvalid = FileAccess::exists(p_preset->get("custom_template/debug"));
            if (!dvalid) {
                template_err += TTR("Custom debug template not found.") + "\n";
            }
        } else {
            has_export_templates |= exists_export_template("android_debug.apk", &template_err);
        }

        if (p_preset->get("custom_template/release") != "") {
            rvalid = FileAccess::exists(p_preset->get("custom_template/release"));
            if (!rvalid) {
                template_err += TTR("Custom release template not found.") + "\n";
            }
        } else {
            has_export_templates |= exists_export_template("android_release.apk", &template_err);
        }

        r_missing_templates = !has_export_templates;
        valid = dvalid || rvalid || has_export_templates;
        if (!valid) {
            err += template_err;
        }
    } else {
        r_missing_templates = !exists_export_template("android_source.zip", &err);

        bool installed_android_build_template = FileAccess::exists("res://android/build/build.gradle");
        if (!installed_android_build_template) {
            err += TTR("Android build template not installed in the project. Install it from the Project menu.") + "\n";
        }

        valid = installed_android_build_template && !r_missing_templates;
    }

    // Validate the rest of the configuration.

    String dk = p_preset->get("keystore/debug");

    if (!FileAccess::exists(dk)) {
        dk = EditorSettings::get_singleton()->get("export/android/debug_keystore");
        if (!FileAccess::exists(dk)) {
            valid = false;
            err += TTR("Debug keystore not configured in the Editor Settings nor in the preset.") + "\n";
        }
    }

    String rk = p_preset->get("keystore/release");

    if (!rk.is_empty() && !FileAccess::exists(rk)) {
        valid = false;
        err += TTR("Release keystore incorrectly configured in the export preset.") + "\n";
    }

    String sdk_path = EditorSettings::get_singleton()->get("export/android/android_sdk_path");
    if (sdk_path == "") {
        err += TTR("A valid Android SDK path is required in Editor Settings.") + "\n";
        valid = false;
    } else {
        Error errn;
        // Check for the platform-tools directory.
        DirAccessRef da = DirAccess::open(sdk_path.plus_file("platform-tools"), &errn);
        if (errn != OK) {
            err += TTR("Invalid Android SDK path in Editor Settings.");
            err += TTR("Missing 'platform-tools' directory!");
            err += "\n";
            valid = false;
        }

        // Validate that adb is available
        String adb_path = get_adb_path();
        if (!FileAccess::exists(adb_path)) {
            err += TTR("Unable to find Android SDK platform-tools' adb command.");
            err += TTR("Please check in the Android SDK directory specified in Editor Settings.");
            err += "\n";
            valid = false;
        }

        // Check for the build-tools directory.
        DirAccessRef build_tools_da = DirAccess::open(sdk_path.plus_file("build-tools"), &errn);
        if (errn != OK) {
            err += TTR("Invalid Android SDK path in Editor Settings.");
            err += TTR("Missing 'build-tools' directory!");
            err += "\n";
            valid = false;
        }

        // Validate that apksigner is available
        String apksigner_path = get_apksigner_path();
        if (!FileAccess::exists(apksigner_path)) {
            err += TTR("Unable to find Android SDK build-tools' apksigner command.");
            err += TTR("Please check in the Android SDK directory specified in Editor Settings.");
            err += "\n";
            valid = false;
        }
    }

    bool apk_expansion = p_preset->get("apk_expansion/enable");

    if (apk_expansion) {
        String apk_expansion_pkey = p_preset->get("apk_expansion/public_key");

        if (apk_expansion_pkey == "") {
            valid = false;

            err += TTR("Invalid public key for APK expansion.") + "\n";
        }
    }

    String pn = p_preset->get("package/unique_name");
    String pn_err;

    if (!is_package_name_valid(get_package_name(pn), &pn_err)) {
        valid = false;
        err += TTR("Invalid package name:") + " " + pn_err + "\n";
    }

    String etc_error = test_etc2();
    if (etc_error != String()) {
        valid = false;
        err += etc_error;
    }

    // Ensure that `Use Custom Build` is enabled if a plugin is selected.
    String enabled_plugins_names = get_plugins_names(get_enabled_plugins(p_preset));
    bool custom_build_enabled = p_preset->get("custom_template/use_custom_build");
    if (!enabled_plugins_names.is_empty() && !custom_build_enabled) {
        valid = false;
        err += TTR("\"Use Custom Build\" must be enabled to use the plugins.");
        err += "\n";
    }

    // Validate the Xr features are properly populated
    int xr_mode_index = p_preset->get("xr_features/xr_mode");
    int hand_tracking = p_preset->get("xr_features/hand_tracking");
    if (xr_mode_index != /* XRMode.OVR*/ 1) {
        if (hand_tracking > 0) {
            valid = false;
            err += TTR("\"Hand Tracking\" is only valid when \"Xr Mode\" is \"Oculus Mobile VR\".");
            err += "\n";
        }
    }

    if (int(p_preset->get("custom_template/export_format")) == EXPORT_FORMAT_AAB &&
            !bool(p_preset->get("custom_template/use_custom_build"))) {
        valid = false;
        err += TTR("\"Export AAB\" is only valid when \"Use Custom Build\" is enabled.");
        err += "\n";
    }

    r_error = err;
    return valid;
}

List<String> EditorExportPlatformAndroidAPK::get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const {
    List<String> list;
    list.push_back("apk");
    list.push_back("aab");
    return list;
}

inline bool EditorExportPlatformAndroidAPK::is_clean_build_required(Vector<PluginConfigAndroid> enabled_plugins) {
    String plugin_names = get_plugins_names(enabled_plugins);
    bool first_build = last_custom_build_time == 0;
    bool have_plugins_changed = false;

    if (!first_build) {
        have_plugins_changed = plugin_names != last_plugin_names;
        if (!have_plugins_changed) {
            for (int i = 0; i < enabled_plugins.size(); i++) {
                if (enabled_plugins.get(i).last_updated > last_custom_build_time) {
                    have_plugins_changed = true;
                    break;
                }
            }
        }
    }

    last_custom_build_time = OS::get_singleton()->get_unix_time();
    last_plugin_names = plugin_names;

    return have_plugins_changed || first_build;
}

String EditorExportPlatformAndroidAPK::get_apk_expansion_fullpath(const Ref<EditorExportPreset> &p_preset, const String &p_path) {
    int version_code = p_preset->get("version/code");
    String package_name = p_preset->get("package/unique_name");
    String apk_file_name = "main." + itos(version_code) + "." + get_package_name(package_name) + ".obb";
    String fullpath = p_path.get_base_dir().plus_file(apk_file_name);
    return fullpath;
}

Error EditorExportPlatformAndroidAPK::save_apk_expansion_file(const Ref<EditorExportPreset> &p_preset, const String &p_path) {
    String fullpath = get_apk_expansion_fullpath(p_preset, p_path);
    Error err = save_pack(p_preset, fullpath);
    return err;
}

void EditorExportPlatformAndroidAPK::get_command_line_flags(const Ref<EditorExportPreset> &p_preset, const String &p_path, int p_flags, Vector<uint8_t> &r_command_line_flags) {
    String cmdline = p_preset->get("command_line/extra_args");
    Vector<String> command_line_strings = cmdline.strip_edges().split(" ");
    for (int i = 0; i < command_line_strings.size(); i++) {
        if (command_line_strings[i].strip_edges().length() == 0) {
            command_line_strings.remove(i);
            i--;
        }
    }

    gen_export_flags(command_line_strings, p_flags);

    bool apk_expansion = p_preset->get("apk_expansion/enable");
    if (apk_expansion) {
        String fullpath = get_apk_expansion_fullpath(p_preset, p_path);
        String apk_expansion_public_key = p_preset->get("apk_expansion/public_key");

        command_line_strings.push_back("--use_apk_expansion");
        command_line_strings.push_back("--apk_expansion_md5");
        command_line_strings.push_back(FileAccess::get_md5(fullpath));
        command_line_strings.push_back("--apk_expansion_key");
        command_line_strings.push_back(apk_expansion_public_key.strip_edges());
    }

    int xr_mode_index = p_preset->get("xr_features/xr_mode");
    if (xr_mode_index == 1) {
        command_line_strings.push_back("--xr_mode_ovr");
    } else { // XRMode.REGULAR is the default.
        command_line_strings.push_back("--xr_mode_regular");
    }

    bool use_32_bit_framebuffer = p_preset->get("graphics/32_bits_framebuffer");
    if (use_32_bit_framebuffer) {
        command_line_strings.push_back("--use_depth_32");
    }

    bool immersive = p_preset->get("screen/immersive_mode");
    if (immersive) {
        command_line_strings.push_back("--use_immersive");
    }

    bool debug_opengl = p_preset->get("graphics/opengl_debug");
    if (debug_opengl) {
        command_line_strings.push_back("--debug_opengl");
    }

    if (command_line_strings.size()) {
        r_command_line_flags.resize(4);
        encode_uint32(command_line_strings.size(), &r_command_line_flags.write[0]);
        for (int i = 0; i < command_line_strings.size(); i++) {
            print_line(itos(i) + " param: " + command_line_strings[i]);
            CharString command_line_argument = command_line_strings[i].utf8();
            int base = r_command_line_flags.size();
            int length = command_line_argument.length();
            if (length == 0) {
                continue;
            }
            r_command_line_flags.resize(base + 4 + length);
            encode_uint32(length, &r_command_line_flags.write[base]);
            memcpy(&r_command_line_flags.write[base + 4], command_line_argument.ptr(), length);
        }
    }
}

Error EditorExportPlatformAndroidAPK::sign_apk(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &export_path, EditorProgress &ep) {
    int export_format = int(p_preset->get("custom_template/export_format"));
    String export_label = export_format == EXPORT_FORMAT_AAB ? "AAB" : "APK";
    String release_keystore = p_preset->get("keystore/release");
    String release_username = p_preset->get("keystore/release_user");
    String release_password = p_preset->get("keystore/release_password");

    String apksigner = get_apksigner_path();
    print_verbose("Starting signing of the " + export_label + " binary using " + apksigner);
    if (!FileAccess::exists(apksigner)) {
        EditorNode::add_io_error("'apksigner' could not be found.\nPlease check the command is available in the Android SDK build-tools directory.\nThe resulting " + export_label + " is unsigned.");
        return OK;
    }

    String keystore;
    String password;
    String user;
    if (p_debug) {
        keystore = p_preset->get("keystore/debug");
        password = p_preset->get("keystore/debug_password");
        user = p_preset->get("keystore/debug_user");

        if (keystore.is_empty()) {
            keystore = EditorSettings::get_singleton()->get("export/android/debug_keystore");
            password = EditorSettings::get_singleton()->get("export/android/debug_keystore_pass");
            user = EditorSettings::get_singleton()->get("export/android/debug_keystore_user");
        }

        if (ep.step("Signing debug " + export_label + "...", 104)) {
            return ERR_SKIP;
        }

    } else {
        keystore = release_keystore;
        password = release_password;
        user = release_username;

        if (ep.step("Signing release " + export_label + "...", 104)) {
            return ERR_SKIP;
        }
    }

    if (!FileAccess::exists(keystore)) {
        EditorNode::add_io_error("Could not find keystore, unable to export.");
        return ERR_FILE_CANT_OPEN;
    }

    String output;
    List<String> args;
    args.push_back("sign");
    args.push_back("--verbose");
    args.push_back("--ks");
    args.push_back(keystore);
    args.push_back("--ks-pass");
    args.push_back("pass:" + password);
    args.push_back("--ks-key-alias");
    args.push_back(user);
    args.push_back(export_path);
    if (p_debug) {
        // We only print verbose logs for debug builds to avoid leaking release keystore credentials.
        print_verbose("Signing debug binary using: " + String("\n") + apksigner + " " + join_list(args, String(" ")));
    }
    int retval;
    output.clear();
    OS::get_singleton()->execute(apksigner, args, &output, &retval, true);
    print_verbose(output);
    if (retval) {
        EditorNode::add_io_error("'apksigner' returned with error #" + itos(retval));
        return ERR_CANT_CREATE;
    }

    if (ep.step("Verifying " + export_label + "...", 105)) {
        return ERR_SKIP;
    }

    args.clear();
    args.push_back("verify");
    args.push_back("--verbose");
    args.push_back(export_path);
    if (p_debug) {
        print_verbose("Verifying signed build using: " + String("\n") + apksigner + " " + join_list(args, String(" ")));
    }

    output.clear();
    OS::get_singleton()->execute(apksigner, args, &output, &retval, true);
    print_verbose(output);
    if (retval) {
        EditorNode::add_io_error("'apksigner' verification of " + export_label + " failed.");
        return ERR_CANT_CREATE;
    }

    print_verbose("Successfully completed signing build.");
    return OK;
}

void EditorExportPlatformAndroidAPK::_clear_assets_directory() {
    DirAccessRef da_res = DirAccess::create(DirAccess::ACCESS_RESOURCES);
    if (da_res->dir_exists("res://android/build/assets")) {
        print_verbose("Clearing assets directory..");
        DirAccessRef da_assets = DirAccess::open("res://android/build/assets");
        da_assets->erase_contents_recursive();
        da_res->remove("res://android/build/assets");
    }
}

String EditorExportPlatformAndroidAPK::join_list(List<String> parts, const String &separator) const {
    String ret;
    for (int i = 0; i < parts.size(); ++i) {
        if (i > 0) {
            ret += separator;
        }
        ret += parts[i];
    }
    return ret;
}

Error EditorExportPlatformAndroidAPK::export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, int p_flags) {
    int export_format = int(p_preset->get("custom_template/export_format"));
    bool should_sign = p_preset->get("package/signed");
    return export_project_helper(p_preset, p_debug, p_path, export_format, should_sign, p_flags);
}

Error EditorExportPlatformAndroidAPK::export_project_helper(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, int export_format, bool should_sign, int p_flags) {
    ExportNotifier notifier(*this, p_preset, p_debug, p_path, p_flags);

    String src_apk;
    Error err;

    EditorProgress ep("export", "Exporting for Android", 105, true);

    bool use_custom_build = bool(p_preset->get("custom_template/use_custom_build"));
    bool p_give_internet = p_flags & (DEBUG_FLAG_DUMB_CLIENT | DEBUG_FLAG_REMOTE_DEBUG);
    bool apk_expansion = p_preset->get("apk_expansion/enable");
    Vector<String> enabled_abis = get_enabled_abis(p_preset);

    print_verbose("Exporting for Android...");
    print_verbose("- debug build: " + bool_to_string(p_debug));
    print_verbose("- export path: " + p_path);
    print_verbose("- export format: " + itos(export_format));
    print_verbose("- sign build: " + bool_to_string(should_sign));
    print_verbose("- custom build enabled: " + bool_to_string(use_custom_build));
    print_verbose("- apk expansion enabled: " + bool_to_string(apk_expansion));
    print_verbose("- enabled abis: " + String(",").join(enabled_abis));
    print_verbose("- export filter: " + itos(p_preset->get_export_filter()));
    print_verbose("- include filter: " + p_preset->get_include_filter());
    print_verbose("- exclude filter: " + p_preset->get_exclude_filter());

    Ref<Image> splash_image;
    Ref<Image> splash_bg_color_image;
    String processed_splash_config_xml = load_splash_refs(splash_image, splash_bg_color_image);

    Ref<Image> main_image;
    Ref<Image> foreground;
    Ref<Image> background;

    load_icon_refs(p_preset, main_image, foreground, background);

    Vector<uint8_t> command_line_flags;
    // Write command line flags into the command_line_flags variable.
    get_command_line_flags(p_preset, p_path, p_flags, command_line_flags);

    if (export_format == EXPORT_FORMAT_AAB) {
        if (!p_path.ends_with(".aab")) {
            EditorNode::get_singleton()->show_warning(TTR("Invalid filename! Android App Bundle requires the *.aab extension."));
            return ERR_UNCONFIGURED;
        }
        if (apk_expansion) {
            EditorNode::get_singleton()->show_warning(TTR("APK Expansion not compatible with Android App Bundle."));
            return ERR_UNCONFIGURED;
        }
    }
    if (export_format == EXPORT_FORMAT_APK && !p_path.ends_with(".apk")) {
        EditorNode::get_singleton()->show_warning(
                TTR("Invalid filename! Android APK requires the *.apk extension."));
        return ERR_UNCONFIGURED;
    }
    if (export_format > EXPORT_FORMAT_AAB || export_format < EXPORT_FORMAT_APK) {
        EditorNode::add_io_error("Unsupported export format!\n");
        return ERR_UNCONFIGURED; //TODO: is this the right error?
    }

    if (use_custom_build) {
        print_verbose("Starting custom build..");
        //test that installed build version is alright
        {
            print_verbose("Checking build version..");
            FileAccessRef f = FileAccess::open("res://android/.build_version", FileAccess::READ);
            if (!f) {
                EditorNode::get_singleton()->show_warning(TTR("Trying to build from a custom built template, but no version info for it exists. Please reinstall from the 'Project' menu."));
                return ERR_UNCONFIGURED;
            }
            String version = f->get_line().strip_edges();
            print_verbose("- build version: " + version);
            f->close();
            if (version != VERSION_FULL_CONFIG) {
                EditorNode::get_singleton()->show_warning(vformat(TTR("Android build version mismatch:\n   Template installed: %s\n   Godot Version: %s\nPlease reinstall Android build template from 'Project' menu."), version, VERSION_FULL_CONFIG));
                return ERR_UNCONFIGURED;
            }
        }
        String sdk_path = EDITOR_GET("export/android/android_sdk_path");
        ERR_FAIL_COND_V_MSG(sdk_path.is_empty(), ERR_UNCONFIGURED, "Android SDK path must be configured in Editor Settings at 'export/android/android_sdk_path'.");
        print_verbose("Android sdk path: " + sdk_path);

        // TODO: should we use "package/name" or "application/config/name"?
        String project_name = get_project_name(p_preset->get("package/name"));
        err = _create_project_name_strings_files(p_preset, project_name); //project name localization.
        if (err != OK) {
            EditorNode::add_io_error("Unable to overwrite res://android/build/res/*.xml files with project name");
        }
        // Copies the project icon files into the appropriate Gradle project directory.
        _copy_icons_to_gradle_project(p_preset, processed_splash_config_xml, splash_image, splash_bg_color_image, main_image, foreground, background);
        // Write an AndroidManifest.xml file into the Gradle project directory.
        _write_tmp_manifest(p_preset, p_give_internet, p_debug);

        //stores all the project files inside the Gradle project directory. Also includes all ABIs
        _clear_assets_directory();
        if (!apk_expansion) {
            print_verbose("Exporting project files..");
            err = export_project_files(p_preset, rename_and_store_file_in_gradle_project, nullptr, ignore_so_file);
            if (err != OK) {
                EditorNode::add_io_error("Could not export project files to gradle project\n");
                return err;
            }
        } else {
            print_verbose("Saving apk expansion file..");
            err = save_apk_expansion_file(p_preset, p_path);
            if (err != OK) {
                EditorNode::add_io_error("Could not write expansion package file!");
                return err;
            }
        }
        print_verbose("Storing command line flags..");
        store_file_at_path("res://android/build/assets/_cl_", command_line_flags);

        print_verbose("Updating ANDROID_HOME environment to " + sdk_path);
        OS::get_singleton()->set_environment("ANDROID_HOME", sdk_path); //set and overwrite if required
        String build_command;

#ifdef WINDOWS_ENABLED
        build_command = "gradlew.bat";
#else
        build_command = "gradlew";
#endif

        String build_path = ProjectSettings::get_singleton()->get_resource_path().plus_file("android/build");
        build_command = build_path.plus_file(build_command);

        String package_name = get_package_name(p_preset->get("package/unique_name"));
        String version_code = itos(p_preset->get("version/code"));
        String version_name = p_preset->get("version/name");
        String enabled_abi_string = String("|").join(enabled_abis);
        String sign_flag = should_sign ? "true" : "false";
        String zipalign_flag = "true";

        Vector<PluginConfigAndroid> enabled_plugins = get_enabled_plugins(p_preset);
        String local_plugins_binaries = get_plugins_binaries(PluginConfigAndroid::BINARY_TYPE_LOCAL, enabled_plugins);
        String remote_plugins_binaries = get_plugins_binaries(PluginConfigAndroid::BINARY_TYPE_REMOTE, enabled_plugins);
        String custom_maven_repos = get_plugins_custom_maven_repos(enabled_plugins);
        bool clean_build_required = is_clean_build_required(enabled_plugins);

        List<String> cmdline;
        if (clean_build_required) {
            cmdline.push_back("clean");
        }

        String build_type = p_debug ? "Debug" : "Release";
        if (export_format == EXPORT_FORMAT_AAB) {
            String bundle_build_command = vformat("bundle%s", build_type);
            cmdline.push_back(bundle_build_command);
        } else if (export_format == EXPORT_FORMAT_APK) {
            String apk_build_command = vformat("assemble%s", build_type);
            cmdline.push_back(apk_build_command);
        }

        cmdline.push_back("-p"); // argument to specify the start directory.
        cmdline.push_back(build_path); // start directory.
        cmdline.push_back("-Pexport_package_name=" + package_name); // argument to specify the package name.
        cmdline.push_back("-Pexport_version_code=" + version_code); // argument to specify the version code.
        cmdline.push_back("-Pexport_version_name=" + version_name); // argument to specify the version name.
        cmdline.push_back("-Pexport_enabled_abis=" + enabled_abi_string); // argument to specify enabled ABIs.
        cmdline.push_back("-Pplugins_local_binaries=" + local_plugins_binaries); // argument to specify the list of plugins local dependencies.
        cmdline.push_back("-Pplugins_remote_binaries=" + remote_plugins_binaries); // argument to specify the list of plugins remote dependencies.
        cmdline.push_back("-Pplugins_maven_repos=" + custom_maven_repos); // argument to specify the list of custom maven repos for the plugins dependencies.
        cmdline.push_back("-Pperform_zipalign=" + zipalign_flag); // argument to specify whether the build should be zipaligned.
        cmdline.push_back("-Pperform_signing=" + sign_flag); // argument to specify whether the build should be signed.
        cmdline.push_back("-Pgodot_editor_version=" + String(VERSION_FULL_CONFIG));

        // NOTE: The release keystore is not included in the verbose logging
        // to avoid accidentally leaking sensitive information when sharing verbose logs for troubleshooting.
        // Any non-sensitive additions to the command line arguments must be done above this section.
        // Sensitive additions must be done below the logging statement.
        print_verbose("Build Android project using gradle command: " + String("\n") + build_command + " " + join_list(cmdline, String(" ")));

        if (should_sign && !p_debug) {
            // Pass the release keystore info as well
            String release_keystore = p_preset->get("keystore/release");
            String release_username = p_preset->get("keystore/release_user");
            String release_password = p_preset->get("keystore/release_password");
            if (!FileAccess::exists(release_keystore)) {
                EditorNode::add_io_error("Could not find keystore, unable to export.");
                return ERR_FILE_CANT_OPEN;
            }

            cmdline.push_back("-Prelease_keystore_file=" + release_keystore); // argument to specify the release keystore file.
            cmdline.push_back("-Prelease_keystore_alias=" + release_username); // argument to specify the release keystore alias.
            cmdline.push_back("-Prelease_keystore_password=" + release_password); // argument to specity the release keystore password.
        }

        int result = EditorNode::get_singleton()->execute_and_show_output(TTR("Building Android Project (gradle)"), build_command, cmdline);
        if (result != 0) {
            EditorNode::get_singleton()->show_warning(TTR("Building of Android project failed, check output for the error.\nAlternatively visit docs.godotengine.org for Android build documentation."));
            return ERR_CANT_CREATE;
        }

        List<String> copy_args;
        String copy_command;
        if (export_format == EXPORT_FORMAT_AAB) {
            copy_command = vformat("copyAndRename%sAab", build_type);
        } else if (export_format == EXPORT_FORMAT_APK) {
            copy_command = vformat("copyAndRename%sApk", build_type);
        }

        copy_args.push_back(copy_command);

        copy_args.push_back("-p"); // argument to specify the start directory.
        copy_args.push_back(build_path); // start directory.

        String export_filename = p_path.get_file();
        String export_path = p_path.get_base_dir();
        if (export_path.is_rel_path()) {
            export_path = OS::get_singleton()->get_resource_dir().plus_file(export_path);
        }
        export_path = ProjectSettings::get_singleton()->globalize_path(export_path).simplify_path();

        copy_args.push_back("-Pexport_path=file:" + export_path);
        copy_args.push_back("-Pexport_filename=" + export_filename);

        print_verbose("Copying Android binary using gradle command: " + String("\n") + build_command + " " + join_list(copy_args, String(" ")));
        int copy_result = EditorNode::get_singleton()->execute_and_show_output(TTR("Moving output"), build_command, copy_args);
        if (copy_result != 0) {
            EditorNode::get_singleton()->show_warning(TTR("Unable to copy and rename export file, check gradle project directory for outputs."));
            return ERR_CANT_CREATE;
        }

        print_verbose("Successfully completed Android custom build.");
        return OK;
    }
    // This is the start of the Legacy build system
    print_verbose("Starting legacy build system..");
    if (p_debug) {
        src_apk = p_preset->get("custom_template/debug");
    } else {
        src_apk = p_preset->get("custom_template/release");
    }
    src_apk = src_apk.strip_edges();
    if (src_apk == "") {
        if (p_debug) {
            src_apk = find_export_template("android_debug.apk");
        } else {
            src_apk = find_export_template("android_release.apk");
        }
        if (src_apk == "") {
            EditorNode::add_io_error("Package not found: " + src_apk);
            return ERR_FILE_NOT_FOUND;
        }
    }

    if (!DirAccess::exists(p_path.get_base_dir())) {
        return ERR_FILE_BAD_PATH;
    }

    FileAccess *src_f = nullptr;
    zlib_filefunc_def io = zipio_create_io_from_file(&src_f);

    if (ep.step("Creating APK...", 0)) {
        return ERR_SKIP;
    }

    unzFile pkg = unzOpen2(src_apk.utf8().get_data(), &io);
    if (!pkg) {
        EditorNode::add_io_error("Could not find template APK to export:\n" + src_apk);
        return ERR_FILE_NOT_FOUND;
    }

    int ret = unzGoToFirstFile(pkg);

    zlib_filefunc_def io2 = io;
    FileAccess *dst_f = nullptr;
    io2.opaque = &dst_f;

    String tmp_unaligned_path = EditorPaths::get_singleton()->get_cache_dir().plus_file("tmpexport-unaligned." + uitos(OS::get_singleton()->get_unix_time()) + ".apk");

#define CLEANUP_AND_RETURN(m_err)                            \
{                                                        \
    DirAccess::remove_file_or_error(tmp_unaligned_path); \
    return m_err;                                        \
}

    zipFile unaligned_apk = zipOpen2(tmp_unaligned_path.utf8().get_data(), APPEND_STATUS_CREATE, nullptr, &io2);

    String cmdline = p_preset->get("command_line/extra_args");

    String version_name = p_preset->get("version/name");
    String package_name = p_preset->get("package/unique_name");

    String apk_expansion_pkey = p_preset->get("apk_expansion/public_key");

    Vector<String> invalid_abis(enabled_abis);
    while (ret == UNZ_OK) {
        //get filename
        unz_file_info info;
        char fname[16384];
        ret = unzGetCurrentFileInfo(pkg, &info, fname, 16384, nullptr, 0, nullptr, 0);

        bool skip = false;

        String file = fname;

        Vector<uint8_t> data;
        data.resize(info.uncompressed_size);

        //read
        unzOpenCurrentFile(pkg);
        unzReadCurrentFile(pkg, data.ptrw(), data.size());
        unzCloseCurrentFile(pkg);

        //write
        if (file == "AndroidManifest.xml") {
            _fix_manifest(p_preset, data, p_give_internet);
        }
        if (file == "resources.arsc") {
            _fix_resources(p_preset, data);
        }

        // Process the splash image
        if ((file == SPLASH_IMAGE_EXPORT_PATH || file == LEGACY_BUILD_SPLASH_IMAGE_EXPORT_PATH) && splash_image.is_valid() && !splash_image->is_empty()) {
            _load_image_data(splash_image, data);
        }

        // Process the splash bg color image
        if ((file == SPLASH_BG_COLOR_PATH || file == LEGACY_BUILD_SPLASH_BG_COLOR_PATH) && splash_bg_color_image.is_valid() && !splash_bg_color_image->is_empty()) {
            _load_image_data(splash_bg_color_image, data);
        }

        for (int i = 0; i < icon_densities_count; ++i) {
            if (main_image.is_valid() && !main_image->is_empty()) {
                if (file == launcher_icons[i].export_path) {
                    _process_launcher_icons(file, main_image, launcher_icons[i].dimensions, data);
                }
            }
            if (foreground.is_valid() && !foreground->is_empty()) {
                if (file == launcher_adaptive_icon_foregrounds[i].export_path) {
                    _process_launcher_icons(file, foreground, launcher_adaptive_icon_foregrounds[i].dimensions, data);
                }
            }
            if (background.is_valid() && !background->is_empty()) {
                if (file == launcher_adaptive_icon_backgrounds[i].export_path) {
                    _process_launcher_icons(file, background, launcher_adaptive_icon_backgrounds[i].dimensions, data);
                }
            }
        }

        if (file.ends_with(".so")) {
            bool enabled = false;
            for (int i = 0; i < enabled_abis.size(); ++i) {
                if (file.begins_with("lib/" + enabled_abis[i] + "/")) {
                    invalid_abis.erase(enabled_abis[i]);
                    enabled = true;
                    break;
                }
            }
            if (!enabled) {
                skip = true;
            }
        }

        if (file.begins_with("META-INF") && should_sign) {
            skip = true;
        }

        if (!skip) {
            print_line("ADDING: " + file);

            // Respect decision on compression made by AAPT for the export template
            const bool uncompressed = info.compression_method == 0;

            zip_fileinfo zipfi = get_zip_fileinfo();

            zipOpenNewFileInZip(unaligned_apk,
                    file.utf8().get_data(),
                    &zipfi,
                    nullptr,
                    0,
                    nullptr,
                    0,
                    nullptr,
                    uncompressed ? 0 : Z_DEFLATED,
                    Z_DEFAULT_COMPRESSION);

            zipWriteInFileInZip(unaligned_apk, data.ptr(), data.size());
            zipCloseFileInZip(unaligned_apk);
        }

        ret = unzGoToNextFile(pkg);
    }

    if (!invalid_abis.is_empty()) {
        String unsupported_arch = String(", ").join(invalid_abis);
        EditorNode::add_io_error("Missing libraries in the export template for the selected architectures: " + unsupported_arch + ".\n" +
                                 "Please build a template with all required libraries, or uncheck the missing architectures in the export preset.");
        CLEANUP_AND_RETURN(ERR_FILE_NOT_FOUND);
    }

    if (ep.step("Adding files...", 1)) {
        CLEANUP_AND_RETURN(ERR_SKIP);
    }
    err = OK;

    if (p_flags & DEBUG_FLAG_DUMB_CLIENT) {
        APKExportData ed;
        ed.ep = &ep;
        ed.apk = unaligned_apk;
        err = export_project_files(p_preset, ignore_apk_file, &ed, save_apk_so);
    } else {
        if (apk_expansion) {
            err = save_apk_expansion_file(p_preset, p_path);
            if (err != OK) {
                EditorNode::add_io_error("Could not write expansion package file!");
                return err;
            }
        } else {
            APKExportData ed;
            ed.ep = &ep;
            ed.apk = unaligned_apk;
            err = export_project_files(p_preset, save_apk_file, &ed, save_apk_so);
        }
    }

    if (err != OK) {
        unzClose(pkg);
        EditorNode::add_io_error("Could not export project files");
        CLEANUP_AND_RETURN(ERR_SKIP);
    }

    zip_fileinfo zipfi = get_zip_fileinfo();
    zipOpenNewFileInZip(unaligned_apk,
            "assets/_cl_",
            &zipfi,
            nullptr,
            0,
            nullptr,
            0,
            nullptr,
            0, // No compress (little size gain and potentially slower startup)
            Z_DEFAULT_COMPRESSION);
    zipWriteInFileInZip(unaligned_apk, command_line_flags.ptr(), command_line_flags.size());
    zipCloseFileInZip(unaligned_apk);
    zipClose(unaligned_apk, nullptr);
    unzClose(pkg);

    if (err != OK) {
        CLEANUP_AND_RETURN(err);
    }

    // Let's zip-align (must be done before signing)

    static const int ZIP_ALIGNMENT = 4;

    // If we're not signing the apk, then the next step should be the last.
    const int next_step = should_sign ? 103 : 105;
    if (ep.step("Aligning APK...", next_step)) {
        CLEANUP_AND_RETURN(ERR_SKIP);
    }

    unzFile tmp_unaligned = unzOpen2(tmp_unaligned_path.utf8().get_data(), &io);
    if (!tmp_unaligned) {
        EditorNode::add_io_error("Could not unzip temporary unaligned APK.");
        CLEANUP_AND_RETURN(ERR_FILE_NOT_FOUND);
    }

    ret = unzGoToFirstFile(tmp_unaligned);

    io2 = io;
    dst_f = nullptr;
    io2.opaque = &dst_f;
    zipFile final_apk = zipOpen2(p_path.utf8().get_data(), APPEND_STATUS_CREATE, nullptr, &io2);

    // Take files from the unaligned APK and write them out to the aligned one
    // in raw mode, i.e. not uncompressing and recompressing, aligning them as needed,
    // following what is done in https://github.com/android/platform_build/blob/master/tools/zipalign/ZipAlign.cpp
    int bias = 0;
    while (ret == UNZ_OK) {
        unz_file_info info;
        memset(&info, 0, sizeof(info));

        char fname[16384];
        char extra[16384];
        ret = unzGetCurrentFileInfo(tmp_unaligned, &info, fname, 16384, extra, 16384 - ZIP_ALIGNMENT, nullptr, 0);

        String file = fname;

        Vector<uint8_t> data;
        data.resize(info.compressed_size);

        // read
        int method, level;
        unzOpenCurrentFile2(tmp_unaligned, &method, &level, 1); // raw read
        long file_offset = unzGetCurrentFileZStreamPos64(tmp_unaligned);
        unzReadCurrentFile(tmp_unaligned, data.ptrw(), data.size());
        unzCloseCurrentFile(tmp_unaligned);

        // align
        int padding = 0;
        if (!info.compression_method) {
            // Uncompressed file => Align
            long new_offset = file_offset + bias;
            padding = (ZIP_ALIGNMENT - (new_offset % ZIP_ALIGNMENT)) % ZIP_ALIGNMENT;
        }

        memset(extra + info.size_file_extra, 0, padding);

        zip_fileinfo fileinfo = get_zip_fileinfo();
        zipOpenNewFileInZip2(final_apk,
                file.utf8().get_data(),
                &fileinfo,
                extra,
                info.size_file_extra + padding,
                nullptr,
                0,
                nullptr,
                method,
                level,
                1); // raw write
        zipWriteInFileInZip(final_apk, data.ptr(), data.size());
        zipCloseFileInZipRaw(final_apk, info.uncompressed_size, info.crc);

        bias += padding;

        ret = unzGoToNextFile(tmp_unaligned);
    }

    zipClose(final_apk, nullptr);
    unzClose(tmp_unaligned);

    if (should_sign) {
        // Signing must be done last as any additional modifications to the
        // file will invalidate the signature.
        err = sign_apk(p_preset, p_debug, p_path, ep);
        if (err != OK) {
            CLEANUP_AND_RETURN(err);
        }
    }

    CLEANUP_AND_RETURN(OK);
}

void EditorExportPlatformAndroidAPK::get_platform_features(List<String> *r_features) {
    r_features->push_back("mobile");
    r_features->push_back("Android");
}

void EditorExportPlatformAndroidAPK::resolve_platform_feature_priorities(const Ref<EditorExportPreset> &p_preset, Set<String> &p_features) {
}

EditorExportPlatformAndroidAPK::EditorExportPlatformAndroidAPK() {
    Ref<Image> img = memnew(Image(_android_logo));
    logo.instance();
    logo->create_from_image(img);

    img = Ref<Image>(memnew(Image(_android_run_icon)));
    run_icon.instance();
    run_icon->create_from_image(img);

    devices_changed.set();
    plugins_changed.set();
    check_for_changes_thread.start(_check_for_changes_poll_thread, this);
}

EditorExportPlatformAndroidAPK::~EditorExportPlatformAndroidAPK() {
    quit_request.set();
    check_for_changes_thread.wait_to_finish();
}
