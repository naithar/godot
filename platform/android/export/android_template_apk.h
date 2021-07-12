/*************************************************************************/
/*  android_template_apk.h                                               */
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

#ifndef ANRDOI_APK_H_
#define ANRDOI_APK_H_

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

#include "android_permissions.h"

class EditorExportPlatformAndroidAPK : public EditorExportPlatform {
    GDCLASS(EditorExportPlatformAndroidAPK, EditorExportPlatform);

    
    
    Ref<ImageTexture> logo;
    Ref<ImageTexture> run_icon;

    struct Device {
        String id;
        String name;
        String description;
        int api_level = 0;
    };

    struct APKExportData {
        zipFile apk;
        EditorProgress *ep = nullptr;
    };

    Vector<PluginConfigAndroid> plugins;
    String last_plugin_names;
    uint64_t last_custom_build_time = 0;
    SafeFlag plugins_changed;
    Mutex plugins_lock;
    Vector<Device> devices;
    SafeFlag devices_changed;
    Mutex device_lock;
    Thread check_for_changes_thread;
    SafeFlag quit_request;

    static void _check_for_changes_poll_thread(void *ud) {
        EditorExportPlatformAndroidAPK *ea = (EditorExportPlatformAndroidAPK *)ud;

        while (!ea->quit_request.is_set()) {
            // Check for plugins updates
            {
                // Nothing to do if we already know the plugins have changed.
                if (!ea->plugins_changed.is_set()) {
                    Vector<PluginConfigAndroid> loaded_plugins = get_plugins();

                    MutexLock lock(ea->plugins_lock);

                    if (ea->plugins.size() != loaded_plugins.size()) {
                        ea->plugins_changed.set();
                    } else {
                        for (int i = 0; i < ea->plugins.size(); i++) {
                            if (ea->plugins[i].name != loaded_plugins[i].name) {
                                ea->plugins_changed.set();
                                break;
                            }
                        }
                    }

                    if (ea->plugins_changed.is_set()) {
                        ea->plugins = loaded_plugins;
                    }
                }
            }

            // Check for devices updates
            String adb = get_adb_path();
            if (FileAccess::exists(adb)) {
                String devices;
                List<String> args;
                args.push_back("devices");
                int ec;
                OS::get_singleton()->execute(adb, args, &devices, &ec);

                Vector<String> ds = devices.split("\n");
                Vector<String> ldevices;
                for (int i = 1; i < ds.size(); i++) {
                    String d = ds[i];
                    int dpos = d.find("device");
                    if (dpos == -1) {
                        continue;
                    }
                    d = d.substr(0, dpos).strip_edges();
                    ldevices.push_back(d);
                }

                MutexLock lock(ea->device_lock);

                bool different = false;

                if (ea->devices.size() != ldevices.size()) {
                    different = true;
                } else {
                    for (int i = 0; i < ea->devices.size(); i++) {
                        if (ea->devices[i].id != ldevices[i]) {
                            different = true;
                            break;
                        }
                    }
                }

                if (different) {
                    Vector<Device> ndevices;

                    for (int i = 0; i < ldevices.size(); i++) {
                        Device d;
                        d.id = ldevices[i];
                        for (int j = 0; j < ea->devices.size(); j++) {
                            if (ea->devices[j].id == ldevices[i]) {
                                d.description = ea->devices[j].description;
                                d.name = ea->devices[j].name;
                                d.api_level = ea->devices[j].api_level;
                            }
                        }

                        if (d.description == "") {
                            //in the oven, request!
                            args.clear();
                            args.push_back("-s");
                            args.push_back(d.id);
                            args.push_back("shell");
                            args.push_back("getprop");
                            int ec2;
                            String dp;

                            OS::get_singleton()->execute(adb, args, &dp, &ec2);

                            Vector<String> props = dp.split("\n");
                            String vendor;
                            String device;
                            d.description = "Device ID: " + d.id + "\n";
                            d.api_level = 0;
                            for (int j = 0; j < props.size(); j++) {
                                // got information by `shell cat /system/build.prop` before and its format is "property=value"
                                // it's now changed to use `shell getporp` because of permission issue with Android 8.0 and above
                                // its format is "[property]: [value]" so changed it as like build.prop
                                String p = props[j];
                                p = p.replace("]: ", "=");
                                p = p.replace("[", "");
                                p = p.replace("]", "");

                                if (p.begins_with("ro.product.model=")) {
                                    device = p.get_slice("=", 1).strip_edges();
                                } else if (p.begins_with("ro.product.brand=")) {
                                    vendor = p.get_slice("=", 1).strip_edges().capitalize();
                                } else if (p.begins_with("ro.build.display.id=")) {
                                    d.description += "Build: " + p.get_slice("=", 1).strip_edges() + "\n";
                                } else if (p.begins_with("ro.build.version.release=")) {
                                    d.description += "Release: " + p.get_slice("=", 1).strip_edges() + "\n";
                                } else if (p.begins_with("ro.build.version.sdk=")) {
                                    d.api_level = p.get_slice("=", 1).to_int();
                                } else if (p.begins_with("ro.product.cpu.abi=")) {
                                    d.description += "CPU: " + p.get_slice("=", 1).strip_edges() + "\n";
                                } else if (p.begins_with("ro.product.manufacturer=")) {
                                    d.description += "Manufacturer: " + p.get_slice("=", 1).strip_edges() + "\n";
                                } else if (p.begins_with("ro.board.platform=")) {
                                    d.description += "Chipset: " + p.get_slice("=", 1).strip_edges() + "\n";
                                } else if (p.begins_with("ro.opengles.version=")) {
                                    uint32_t opengl = p.get_slice("=", 1).to_int();
                                    d.description += "OpenGL: " + itos(opengl >> 16) + "." + itos((opengl >> 8) & 0xFF) + "." + itos((opengl)&0xFF) + "\n";
                                }
                            }

                            d.name = vendor + " " + device;
                            if (device == String()) {
                                continue;
                            }
                        }

                        ndevices.push_back(d);
                    }

                    ea->devices = ndevices;
                    ea->devices_changed.set();
                }
            }

            uint64_t sleep = 200;
            uint64_t wait = 3000000;
            uint64_t time = OS::get_singleton()->get_ticks_usec();
            while (OS::get_singleton()->get_ticks_usec() - time < wait) {
                OS::get_singleton()->delay_usec(1000 * sleep);
                if (ea->quit_request.is_set()) {
                    break;
                }
            }
        }

        if (EditorSettings::get_singleton()->get("export/android/shutdown_adb_on_exit")) {
            String adb = get_adb_path();
            if (!FileAccess::exists(adb)) {
                return; //adb not configured
            }

            List<String> args;
            args.push_back("kill-server");
            OS::get_singleton()->execute(adb, args);
        };
    }

    String get_project_name(const String &p_name) const {
        String aname;
        if (p_name != "") {
            aname = p_name;
        } else {
            aname = ProjectSettings::get_singleton()->get("application/config/name");
        }

        if (aname == "") {
            aname = VERSION_NAME;
        }

        return aname;
    }

    String get_package_name(const String &p_package) const {
        String pname = p_package;
        String basename = ProjectSettings::get_singleton()->get("application/config/name");
        basename = basename.to_lower();

        String name;
        bool first = true;
        for (int i = 0; i < basename.length(); i++) {
            char32_t c = basename[i];
            if (c >= '0' && c <= '9' && first) {
                continue;
            }
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
                name += String::chr(c);
                first = false;
            }
        }
        if (name == "") {
            name = "noname";
        }

        pname = pname.replace("$genname", name);

        return pname;
    }

    bool is_package_name_valid(const String &p_package, String *r_error = nullptr) const {
        String pname = p_package;

        if (pname.length() == 0) {
            if (r_error) {
                *r_error = TTR("Package name is missing.");
            }
            return false;
        }

        int segments = 0;
        bool first = true;
        for (int i = 0; i < pname.length(); i++) {
            char32_t c = pname[i];
            if (first && c == '.') {
                if (r_error) {
                    *r_error = TTR("Package segments must be of non-zero length.");
                }
                return false;
            }
            if (c == '.') {
                segments++;
                first = true;
                continue;
            }
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) {
                if (r_error) {
                    *r_error = vformat(TTR("The character '%s' is not allowed in Android application package names."), String::chr(c));
                }
                return false;
            }
            if (first && (c >= '0' && c <= '9')) {
                if (r_error) {
                    *r_error = TTR("A digit cannot be the first character in a package segment.");
                }
                return false;
            }
            if (first && c == '_') {
                if (r_error) {
                    *r_error = vformat(TTR("The character '%s' cannot be the first character in a package segment."), String::chr(c));
                }
                return false;
            }
            first = false;
        }

        if (segments == 0) {
            if (r_error) {
                *r_error = TTR("The package must have at least one '.' separator.");
            }
            return false;
        }

        if (first) {
            if (r_error) {
                *r_error = TTR("Package segments must be of non-zero length.");
            }
            return false;
        }

        return true;
    }

    static bool _should_compress_asset(const String &p_path, const Vector<uint8_t> &p_data) {
        /*
         *  By not compressing files with little or not benefit in doing so,
         *  a performance gain is expected attime. Moreover, if the APK is
         *  zip-aligned, assets stored as they are can be efficiently read by
         *  Android by memory-mapping them.
         */

        // -- Unconditional uncompress to mimic AAPT plus some other

        static const char *unconditional_compress_ext[] = {
            // From https://github.com/android/platform_frameworks_base/blob/master/tools/aapt/Package.cpp
            // These formats are already compressed, or don't compress well:
            ".jpg", ".jpeg", ".png", ".gif",
            ".wav", ".mp2", ".mp3", ".ogg", ".aac",
            ".mpg", ".mpeg", ".mid", ".midi", ".smf", ".jet",
            ".rtttl", ".imy", ".xmf", ".mp4", ".m4a",
            ".m4v", ".3gp", ".3gpp", ".3g2", ".3gpp2",
            ".amr", ".awb", ".wma", ".wmv",
            // Godot-specific:
            ".webp", // Same reasoning as .png
            ".cfb", // Don't let small config files slow-down startup
            ".scn", // Binary scenes are usually already compressed
            ".stex", // Streamable textures are usually already compressed
            // Trailer for easier processing
            nullptr
        };

        for (const char **ext = unconditional_compress_ext; *ext; ++ext) {
            if (p_path.to_lower().ends_with(String(*ext))) {
                return false;
            }
        }

        // -- Compressed resource?

        if (p_data.size() >= 4 && p_data[0] == 'R' && p_data[1] == 'S' && p_data[2] == 'C' && p_data[3] == 'C') {
            // Already compressed
            return false;
        }

        // --- TODO: Decide on texture resources according to their image compression setting

        return true;
    }

    static zip_fileinfo get_zip_fileinfo() {
        OS::Time time = OS::get_singleton()->get_time();
        OS::Date date = OS::get_singleton()->get_date();

        zip_fileinfo zipfi;
        zipfi.tmz_date.tm_hour = time.hour;
        zipfi.tmz_date.tm_mday = date.day;
        zipfi.tmz_date.tm_min = time.min;
        zipfi.tmz_date.tm_mon = date.month - 1; // tm_mon is zero indexed
        zipfi.tmz_date.tm_sec = time.sec;
        zipfi.tmz_date.tm_year = date.year;
        zipfi.dosDate = 0;
        zipfi.external_fa = 0;
        zipfi.internal_fa = 0;

        return zipfi;
    }

    static Vector<String> get_abis() {
        Vector<String> abis;
        abis.push_back("armeabi-v7a");
        abis.push_back("arm64-v8a");
        abis.push_back("x86");
        abis.push_back("x86_64");
        return abis;
    }

    /// List the gdap files in the directory specified by the p_path parameter.
    static Vector<String> list_gdap_files(const String &p_path) {
        Vector<String> dir_files;
        DirAccessRef da = DirAccess::open(p_path);
        if (da) {
            da->list_dir_begin();
            while (true) {
                String file = da->get_next();
                if (file == "") {
                    break;
                }

                if (da->current_is_dir() || da->current_is_hidden()) {
                    continue;
                }

                if (file.ends_with(PluginConfigAndroid::PLUGIN_CONFIG_EXT)) {
                    dir_files.push_back(file);
                }
            }
            da->list_dir_end();
        }

        return dir_files;
    }

    static Vector<PluginConfigAndroid> get_plugins() {
        Vector<PluginConfigAndroid> loaded_plugins;

        String plugins_dir = ProjectSettings::get_singleton()->get_resource_path().plus_file("android/plugins");

        // Add the prebuilt plugins
        loaded_plugins.append_array(get_prebuilt_plugins(plugins_dir));

        if (DirAccess::exists(plugins_dir)) {
            Vector<String> plugins_filenames = list_gdap_files(plugins_dir);

            if (!plugins_filenames.is_empty()) {
                Ref<ConfigFile> config_file = memnew(ConfigFile);
                for (int i = 0; i < plugins_filenames.size(); i++) {
                    PluginConfigAndroid config = load_plugin_config(config_file, plugins_dir.plus_file(plugins_filenames[i]));
                    if (config.valid_config) {
                        loaded_plugins.push_back(config);
                    } else {
                        print_error("Invalid plugin config file " + plugins_filenames[i]);
                    }
                }
            }
        }

        return loaded_plugins;
    }

    static Vector<PluginConfigAndroid> get_enabled_plugins(const Ref<EditorExportPreset> &p_presets) {
        Vector<PluginConfigAndroid> enabled_plugins;
        Vector<PluginConfigAndroid> all_plugins = get_plugins();
        for (int i = 0; i < all_plugins.size(); i++) {
            PluginConfigAndroid plugin = all_plugins[i];
            bool enabled = p_presets->get("plugins/" + plugin.name);
            if (enabled) {
                enabled_plugins.push_back(plugin);
            }
        }

        return enabled_plugins;
    }

    static Error store_in_apk(APKExportData *ed, const String &p_path, const Vector<uint8_t> &p_data, int compression_method = Z_DEFLATED) {
        zip_fileinfo zipfi = get_zip_fileinfo();
        zipOpenNewFileInZip(ed->apk,
                p_path.utf8().get_data(),
                &zipfi,
                nullptr,
                0,
                nullptr,
                0,
                nullptr,
                compression_method,
                Z_DEFAULT_COMPRESSION);

        zipWriteInFileInZip(ed->apk, p_data.ptr(), p_data.size());
        zipCloseFileInZip(ed->apk);

        return OK;
    }

    static Error save_apk_so(void *p_userdata, const SharedObject &p_so) {
        if (!p_so.path.get_file().begins_with("lib")) {
            String err = "Android .so file names must start with \"lib\", but got: " + p_so.path;
            ERR_PRINT(err);
            return FAILED;
        }
        APKExportData *ed = (APKExportData *)p_userdata;
        Vector<String> abis = get_abis();
        bool exported = false;
        for (int i = 0; i < p_so.tags.size(); ++i) {
            // shared objects can be fat (compatible with multiple ABIs)
            int abi_index = abis.find(p_so.tags[i]);
            if (abi_index != -1) {
                exported = true;
                String abi = abis[abi_index];
                String dst_path = String("lib").plus_file(abi).plus_file(p_so.path.get_file());
                Vector<uint8_t> array = FileAccess::get_file_as_array(p_so.path);
                Error store_err = store_in_apk(ed, dst_path, array);
                ERR_FAIL_COND_V_MSG(store_err, store_err, "Cannot store in apk file '" + dst_path + "'.");
            }
        }
        if (!exported) {
            String abis_string = String(" ").join(abis);
            String err = "Cannot determine ABI for library \"" + p_so.path + "\". One of the supported ABIs must be used as a tag: " + abis_string;
            ERR_PRINT(err);
            return FAILED;
        }
        return OK;
    }

    static Error save_apk_file(void *p_userdata, const String &p_path, const Vector<uint8_t> &p_data, int p_file, int p_total, const Vector<String> &p_enc_in_filters, const Vector<String> &p_enc_ex_filters, const Vector<uint8_t> &p_key) {
        APKExportData *ed = (APKExportData *)p_userdata;
        String dst_path = p_path.replace_first("res://", "assets/");

        store_in_apk(ed, dst_path, p_data, _should_compress_asset(p_path, p_data) ? Z_DEFLATED : 0);
        return OK;
    }

    static Error ignore_apk_file(void *p_userdata, const String &p_path, const Vector<uint8_t> &p_data, int p_file, int p_total, const Vector<String> &p_enc_in_filters, const Vector<String> &p_enc_ex_filters, const Vector<uint8_t> &p_key) {
        return OK;
    }

    void _get_permissions(const Ref<EditorExportPreset> &p_preset, bool p_give_internet, Vector<String> &r_permissions) {
        const char **aperms = android_perms;
        while (*aperms) {
            bool enabled = p_preset->get("permissions/" + String(*aperms).to_lower());
            if (enabled) {
                r_permissions.push_back("android.permission." + String(*aperms));
            }
            aperms++;
        }
        PackedStringArray user_perms = p_preset->get("permissions/custom_permissions");
        for (int i = 0; i < user_perms.size(); i++) {
            String user_perm = user_perms[i].strip_edges();
            if (!user_perm.is_empty()) {
                r_permissions.push_back(user_perm);
            }
        }
        if (p_give_internet) {
            if (r_permissions.find("android.permission.INTERNET") == -1) {
                r_permissions.push_back("android.permission.INTERNET");
            }
        }

        int xr_mode_index = p_preset->get("xr_features/xr_mode");
        if (xr_mode_index == 1 /* XRMode.OVR */) {
            int hand_tracking_index = p_preset->get("xr_features/hand_tracking"); // 0: none, 1: optional, 2: required
            if (hand_tracking_index > 0) {
                if (r_permissions.find("com.oculus.permission.HAND_TRACKING") == -1) {
                    r_permissions.push_back("com.oculus.permission.HAND_TRACKING");
                }
            }
        }
    }

    void _write_tmp_manifest(const Ref<EditorExportPreset> &p_preset, bool p_give_internet, bool p_debug) {
        print_verbose("Building temporary manifest..");
        String manifest_text =
                "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                "<manifest xmlns:android=\"http://schemas.android.com/apk/res/android\"\n"
                "    xmlns:tools=\"http://schemas.android.com/tools\">\n";

        manifest_text += _get_screen_sizes_tag(p_preset);
        manifest_text += _get_gles_tag();

        Vector<String> perms;
        _get_permissions(p_preset, p_give_internet, perms);
        for (int i = 0; i < perms.size(); i++) {
            manifest_text += vformat("    <uses-permission android:name=\"%s\" />\n", perms.get(i));
        }

        manifest_text += _get_xr_features_tag(p_preset);
        manifest_text += _get_instrumentation_tag(p_preset);
        manifest_text += _get_application_tag(p_preset);
        manifest_text += "</manifest>\n";
        String manifest_path = vformat("res://android/build/src/%s/AndroidManifest.xml", (p_debug ? "debug" : "release"));

        print_verbose("Storing manifest into " + manifest_path + ": " + "\n" + manifest_text);
        store_string_at_path(manifest_path, manifest_text);
    }

    void _fix_manifest(const Ref<EditorExportPreset> &p_preset, Vector<uint8_t> &p_manifest, bool p_give_internet) {
        // Leaving the unused types commented because looking these constants up
        // again later would be annoying
        // const int CHUNK_AXML_FILE = 0x00080003;
        // const int CHUNK_RESOURCEIDS = 0x00080180;
        const int CHUNK_STRINGS = 0x001C0001;
        // const int CHUNK_XML_END_NAMESPACE = 0x00100101;
        const int CHUNK_XML_END_TAG = 0x00100103;
        // const int CHUNK_XML_START_NAMESPACE = 0x00100100;
        const int CHUNK_XML_START_TAG = 0x00100102;
        // const int CHUNK_XML_TEXT = 0x00100104;
        const int UTF8_FLAG = 0x00000100;

        Vector<String> string_table;

        uint32_t ofs = 8;

        uint32_t string_count = 0;
        //uint32_t styles_count = 0;
        uint32_t string_flags = 0;
        uint32_t string_data_offset = 0;

        //uint32_t styles_offset = 0;
        uint32_t string_table_begins = 0;
        uint32_t string_table_ends = 0;
        Vector<uint8_t> stable_extra;

        String version_name = p_preset->get("version/name");
        int version_code = p_preset->get("version/code");
        String package_name = p_preset->get("package/unique_name");

        const int screen_orientation =
                _get_android_orientation_value(DisplayServer::ScreenOrientation(int(GLOBAL_GET("display/window/handheld/orientation"))));

        bool screen_support_small = p_preset->get("screen/support_small");
        bool screen_support_normal = p_preset->get("screen/support_normal");
        bool screen_support_large = p_preset->get("screen/support_large");
        bool screen_support_xlarge = p_preset->get("screen/support_xlarge");

        int xr_mode_index = p_preset->get("xr_features/xr_mode");

        bool backup_allowed = p_preset->get("user_data_backup/allow");

        Vector<String> perms;
        // Write permissions into the perms variable.
        _get_permissions(p_preset, p_give_internet, perms);

        while (ofs < (uint32_t)p_manifest.size()) {
            uint32_t chunk = decode_uint32(&p_manifest[ofs]);
            uint32_t size = decode_uint32(&p_manifest[ofs + 4]);

            switch (chunk) {
                case CHUNK_STRINGS: {
                    int iofs = ofs + 8;

                    string_count = decode_uint32(&p_manifest[iofs]);
                    //styles_count = decode_uint32(&p_manifest[iofs + 4]);
                    string_flags = decode_uint32(&p_manifest[iofs + 8]);
                    string_data_offset = decode_uint32(&p_manifest[iofs + 12]);
                    //styles_offset = decode_uint32(&p_manifest[iofs + 16]);
                    /*
                    printf("string count: %i\n",string_count);
                    printf("flags: %i\n",string_flags);
                    printf("sdata ofs: %i\n",string_data_offset);
                    printf("styles ofs: %i\n",styles_offset);
                    */
                    uint32_t st_offset = iofs + 20;
                    string_table.resize(string_count);
                    uint32_t string_end = 0;

                    string_table_begins = st_offset;

                    for (uint32_t i = 0; i < string_count; i++) {
                        uint32_t string_at = decode_uint32(&p_manifest[st_offset + i * 4]);
                        string_at += st_offset + string_count * 4;

                        ERR_FAIL_COND_MSG(string_flags & UTF8_FLAG, "Unimplemented, can't read UTF-8 string table.");

                        if (string_flags & UTF8_FLAG) {
                        } else {
                            uint32_t len = decode_uint16(&p_manifest[string_at]);
                            Vector<char32_t> ucstring;
                            ucstring.resize(len + 1);
                            for (uint32_t j = 0; j < len; j++) {
                                uint16_t c = decode_uint16(&p_manifest[string_at + 2 + 2 * j]);
                                ucstring.write[j] = c;
                            }
                            string_end = MAX(string_at + 2 + 2 * len, string_end);
                            ucstring.write[len] = 0;
                            string_table.write[i] = ucstring.ptr();
                        }
                    }

                    for (uint32_t i = string_end; i < (ofs + size); i++) {
                        stable_extra.push_back(p_manifest[i]);
                    }

                    string_table_ends = ofs + size;

                } break;
                case CHUNK_XML_START_TAG: {
                    int iofs = ofs + 8;
                    uint32_t name = decode_uint32(&p_manifest[iofs + 12]);

                    String tname = string_table[name];
                    uint32_t attrcount = decode_uint32(&p_manifest[iofs + 20]);
                    iofs += 28;

                    for (uint32_t i = 0; i < attrcount; i++) {
                        uint32_t attr_nspace = decode_uint32(&p_manifest[iofs]);
                        uint32_t attr_name = decode_uint32(&p_manifest[iofs + 4]);
                        uint32_t attr_value = decode_uint32(&p_manifest[iofs + 8]);
                        uint32_t attr_resid = decode_uint32(&p_manifest[iofs + 16]);

                        const String value = (attr_value != 0xFFFFFFFF) ? string_table[attr_value] : "Res #" + itos(attr_resid);
                        String attrname = string_table[attr_name];
                        const String nspace = (attr_nspace != 0xFFFFFFFF) ? string_table[attr_nspace] : "";

                        //replace project information
                        if (tname == "manifest" && attrname == "package") {
                            string_table.write[attr_value] = get_package_name(package_name);
                        }

                        if (tname == "manifest" && attrname == "versionCode") {
                            encode_uint32(version_code, &p_manifest.write[iofs + 16]);
                        }

                        if (tname == "manifest" && attrname == "versionName") {
                            if (attr_value == 0xFFFFFFFF) {
                                WARN_PRINT("Version name in a resource, should be plain text");
                            } else {
                                string_table.write[attr_value] = version_name;
                            }
                        }

                        if (tname == "application" && attrname == "allowBackup") {
                            encode_uint32(backup_allowed, &p_manifest.write[iofs + 16]);
                        }

                        if (tname == "instrumentation" && attrname == "targetPackage") {
                            string_table.write[attr_value] = get_package_name(package_name);
                        }

                        if (tname == "activity" && attrname == "screenOrientation") {
                            encode_uint32(screen_orientation, &p_manifest.write[iofs + 16]);
                        }

                        if (tname == "supports-screens") {
                            if (attrname == "smallScreens") {
                                encode_uint32(screen_support_small ? 0xFFFFFFFF : 0, &p_manifest.write[iofs + 16]);

                            } else if (attrname == "normalScreens") {
                                encode_uint32(screen_support_normal ? 0xFFFFFFFF : 0, &p_manifest.write[iofs + 16]);

                            } else if (attrname == "largeScreens") {
                                encode_uint32(screen_support_large ? 0xFFFFFFFF : 0, &p_manifest.write[iofs + 16]);

                            } else if (attrname == "xlargeScreens") {
                                encode_uint32(screen_support_xlarge ? 0xFFFFFFFF : 0, &p_manifest.write[iofs + 16]);
                            }
                        }

                        iofs += 20;
                    }

                } break;
                case CHUNK_XML_END_TAG: {
                    int iofs = ofs + 8;
                    uint32_t name = decode_uint32(&p_manifest[iofs + 12]);
                    String tname = string_table[name];

                    if (tname == "uses-feature") {
                        Vector<String> feature_names;
                        Vector<bool> feature_required_list;
                        Vector<int> feature_versions;

                        if (xr_mode_index == 1 /* XRMode.OVR */) {
                            // Check for hand tracking
                            int hand_tracking_index = p_preset->get("xr_features/hand_tracking"); // 0: none, 1: optional, 2: required
                            if (hand_tracking_index > 0) {
                                feature_names.push_back("oculus.software.handtracking");
                                feature_required_list.push_back(hand_tracking_index == 2);
                                feature_versions.push_back(-1); // no version attribute should be added.
                            }
                        }

                        if (feature_names.size() > 0) {
                            ofs += 24; // skip over end tag

                            // save manifest ending so we can restore it
                            Vector<uint8_t> manifest_end;
                            uint32_t manifest_cur_size = p_manifest.size();

                            manifest_end.resize(p_manifest.size() - ofs);
                            memcpy(manifest_end.ptrw(), &p_manifest[ofs], manifest_end.size());

                            int32_t attr_name_string = string_table.find("name");
                            ERR_FAIL_COND_MSG(attr_name_string == -1, "Template does not have 'name' attribute.");

                            int32_t ns_android_string = string_table.find("http://schemas.android.com/apk/res/android");
                            if (ns_android_string == -1) {
                                string_table.push_back("http://schemas.android.com/apk/res/android");
                                ns_android_string = string_table.size() - 1;
                            }

                            int32_t attr_uses_feature_string = string_table.find("uses-feature");
                            if (attr_uses_feature_string == -1) {
                                string_table.push_back("uses-feature");
                                attr_uses_feature_string = string_table.size() - 1;
                            }

                            int32_t attr_required_string = string_table.find("required");
                            if (attr_required_string == -1) {
                                string_table.push_back("required");
                                attr_required_string = string_table.size() - 1;
                            }

                            for (int i = 0; i < feature_names.size(); i++) {
                                String feature_name = feature_names[i];
                                bool feature_required = feature_required_list[i];
                                int feature_version = feature_versions[i];
                                bool has_version_attribute = feature_version != -1;

                                print_line("Adding feature " + feature_name);

                                int32_t feature_string = string_table.find(feature_name);
                                if (feature_string == -1) {
                                    string_table.push_back(feature_name);
                                    feature_string = string_table.size() - 1;
                                }

                                String required_value_string = feature_required ? "true" : "false";
                                int32_t required_value = string_table.find(required_value_string);
                                if (required_value == -1) {
                                    string_table.push_back(required_value_string);
                                    required_value = string_table.size() - 1;
                                }

                                int32_t attr_version_string = -1;
                                int32_t version_value = -1;
                                int tag_size;
                                int attr_count;
                                if (has_version_attribute) {
                                    attr_version_string = string_table.find("version");
                                    if (attr_version_string == -1) {
                                        string_table.push_back("version");
                                        attr_version_string = string_table.size() - 1;
                                    }

                                    version_value = string_table.find(itos(feature_version));
                                    if (version_value == -1) {
                                        string_table.push_back(itos(feature_version));
                                        version_value = string_table.size() - 1;
                                    }

                                    tag_size = 96; // node and three attrs + end node
                                    attr_count = 3;
                                } else {
                                    tag_size = 76; // node and two attrs + end node
                                    attr_count = 2;
                                }
                                manifest_cur_size += tag_size + 24;
                                p_manifest.resize(manifest_cur_size);

                                // start tag
                                encode_uint16(0x102, &p_manifest.write[ofs]); // type
                                encode_uint16(16, &p_manifest.write[ofs + 2]); // headersize
                                encode_uint32(tag_size, &p_manifest.write[ofs + 4]); // size
                                encode_uint32(0, &p_manifest.write[ofs + 8]); // lineno
                                encode_uint32(-1, &p_manifest.write[ofs + 12]); // comment
                                encode_uint32(-1, &p_manifest.write[ofs + 16]); // ns
                                encode_uint32(attr_uses_feature_string, &p_manifest.write[ofs + 20]); // name
                                encode_uint16(20, &p_manifest.write[ofs + 24]); // attr_start
                                encode_uint16(20, &p_manifest.write[ofs + 26]); // attr_size
                                encode_uint16(attr_count, &p_manifest.write[ofs + 28]); // num_attrs
                                encode_uint16(0, &p_manifest.write[ofs + 30]); // id_index
                                encode_uint16(0, &p_manifest.write[ofs + 32]); // class_index
                                encode_uint16(0, &p_manifest.write[ofs + 34]); // style_index

                                // android:name attribute
                                encode_uint32(ns_android_string, &p_manifest.write[ofs + 36]); // ns
                                encode_uint32(attr_name_string, &p_manifest.write[ofs + 40]); // 'name'
                                encode_uint32(feature_string, &p_manifest.write[ofs + 44]); // raw_value
                                encode_uint16(8, &p_manifest.write[ofs + 48]); // typedvalue_size
                                p_manifest.write[ofs + 50] = 0; // typedvalue_always0
                                p_manifest.write[ofs + 51] = 0x03; // typedvalue_type (string)
                                encode_uint32(feature_string, &p_manifest.write[ofs + 52]); // typedvalue reference

                                // android:required attribute
                                encode_uint32(ns_android_string, &p_manifest.write[ofs + 56]); // ns
                                encode_uint32(attr_required_string, &p_manifest.write[ofs + 60]); // 'name'
                                encode_uint32(required_value, &p_manifest.write[ofs + 64]); // raw_value
                                encode_uint16(8, &p_manifest.write[ofs + 68]); // typedvalue_size
                                p_manifest.write[ofs + 70] = 0; // typedvalue_always0
                                p_manifest.write[ofs + 71] = 0x03; // typedvalue_type (string)
                                encode_uint32(required_value, &p_manifest.write[ofs + 72]); // typedvalue reference

                                ofs += 76;

                                if (has_version_attribute) {
                                    // android:version attribute
                                    encode_uint32(ns_android_string, &p_manifest.write[ofs]); // ns
                                    encode_uint32(attr_version_string, &p_manifest.write[ofs + 4]); // 'name'
                                    encode_uint32(version_value, &p_manifest.write[ofs + 8]); // raw_value
                                    encode_uint16(8, &p_manifest.write[ofs + 12]); // typedvalue_size
                                    p_manifest.write[ofs + 14] = 0; // typedvalue_always0
                                    p_manifest.write[ofs + 15] = 0x03; // typedvalue_type (string)
                                    encode_uint32(version_value, &p_manifest.write[ofs + 16]); // typedvalue reference

                                    ofs += 20;
                                }

                                // end tag
                                encode_uint16(0x103, &p_manifest.write[ofs]); // type
                                encode_uint16(16, &p_manifest.write[ofs + 2]); // headersize
                                encode_uint32(24, &p_manifest.write[ofs + 4]); // size
                                encode_uint32(0, &p_manifest.write[ofs + 8]); // lineno
                                encode_uint32(-1, &p_manifest.write[ofs + 12]); // comment
                                encode_uint32(-1, &p_manifest.write[ofs + 16]); // ns
                                encode_uint32(attr_uses_feature_string, &p_manifest.write[ofs + 20]); // name

                                ofs += 24;
                            }
                            memcpy(&p_manifest.write[ofs], manifest_end.ptr(), manifest_end.size());
                            ofs -= 24; // go back over back end
                        }
                    }
                    if (tname == "manifest") {
                        // save manifest ending so we can restore it
                        Vector<uint8_t> manifest_end;
                        uint32_t manifest_cur_size = p_manifest.size();

                        manifest_end.resize(p_manifest.size() - ofs);
                        memcpy(manifest_end.ptrw(), &p_manifest[ofs], manifest_end.size());

                        int32_t attr_name_string = string_table.find("name");
                        ERR_FAIL_COND_MSG(attr_name_string == -1, "Template does not have 'name' attribute.");

                        int32_t ns_android_string = string_table.find("android");
                        ERR_FAIL_COND_MSG(ns_android_string == -1, "Template does not have 'android' namespace.");

                        int32_t attr_uses_permission_string = string_table.find("uses-permission");
                        if (attr_uses_permission_string == -1) {
                            string_table.push_back("uses-permission");
                            attr_uses_permission_string = string_table.size() - 1;
                        }

                        for (int i = 0; i < perms.size(); ++i) {
                            print_line("Adding permission " + perms[i]);

                            manifest_cur_size += 56 + 24; // node + end node
                            p_manifest.resize(manifest_cur_size);

                            // Add permission to the string pool
                            int32_t perm_string = string_table.find(perms[i]);
                            if (perm_string == -1) {
                                string_table.push_back(perms[i]);
                                perm_string = string_table.size() - 1;
                            }

                            // start tag
                            encode_uint16(0x102, &p_manifest.write[ofs]); // type
                            encode_uint16(16, &p_manifest.write[ofs + 2]); // headersize
                            encode_uint32(56, &p_manifest.write[ofs + 4]); // size
                            encode_uint32(0, &p_manifest.write[ofs + 8]); // lineno
                            encode_uint32(-1, &p_manifest.write[ofs + 12]); // comment
                            encode_uint32(-1, &p_manifest.write[ofs + 16]); // ns
                            encode_uint32(attr_uses_permission_string, &p_manifest.write[ofs + 20]); // name
                            encode_uint16(20, &p_manifest.write[ofs + 24]); // attr_start
                            encode_uint16(20, &p_manifest.write[ofs + 26]); // attr_size
                            encode_uint16(1, &p_manifest.write[ofs + 28]); // num_attrs
                            encode_uint16(0, &p_manifest.write[ofs + 30]); // id_index
                            encode_uint16(0, &p_manifest.write[ofs + 32]); // class_index
                            encode_uint16(0, &p_manifest.write[ofs + 34]); // style_index

                            // attribute
                            encode_uint32(ns_android_string, &p_manifest.write[ofs + 36]); // ns
                            encode_uint32(attr_name_string, &p_manifest.write[ofs + 40]); // 'name'
                            encode_uint32(perm_string, &p_manifest.write[ofs + 44]); // raw_value
                            encode_uint16(8, &p_manifest.write[ofs + 48]); // typedvalue_size
                            p_manifest.write[ofs + 50] = 0; // typedvalue_always0
                            p_manifest.write[ofs + 51] = 0x03; // typedvalue_type (string)
                            encode_uint32(perm_string, &p_manifest.write[ofs + 52]); // typedvalue reference

                            ofs += 56;

                            // end tag
                            encode_uint16(0x103, &p_manifest.write[ofs]); // type
                            encode_uint16(16, &p_manifest.write[ofs + 2]); // headersize
                            encode_uint32(24, &p_manifest.write[ofs + 4]); // size
                            encode_uint32(0, &p_manifest.write[ofs + 8]); // lineno
                            encode_uint32(-1, &p_manifest.write[ofs + 12]); // comment
                            encode_uint32(-1, &p_manifest.write[ofs + 16]); // ns
                            encode_uint32(attr_uses_permission_string, &p_manifest.write[ofs + 20]); // name

                            ofs += 24;
                        }

                        // copy footer back in
                        memcpy(&p_manifest.write[ofs], manifest_end.ptr(), manifest_end.size());
                    }
                } break;
            }

            ofs += size;
        }

        //create new andriodmanifest binary

        Vector<uint8_t> ret;
        ret.resize(string_table_begins + string_table.size() * 4);

        for (uint32_t i = 0; i < string_table_begins; i++) {
            ret.write[i] = p_manifest[i];
        }

        ofs = 0;
        for (int i = 0; i < string_table.size(); i++) {
            encode_uint32(ofs, &ret.write[string_table_begins + i * 4]);
            ofs += string_table[i].length() * 2 + 2 + 2;
        }

        ret.resize(ret.size() + ofs);
        string_data_offset = ret.size() - ofs;
        uint8_t *chars = &ret.write[string_data_offset];
        for (int i = 0; i < string_table.size(); i++) {
            String s = string_table[i];
            encode_uint16(s.length(), chars);
            chars += 2;
            for (int j = 0; j < s.length(); j++) {
                encode_uint16(s[j], chars);
                chars += 2;
            }
            encode_uint16(0, chars);
            chars += 2;
        }

        for (int i = 0; i < stable_extra.size(); i++) {
            ret.push_back(stable_extra[i]);
        }

        //pad
        while (ret.size() % 4) {
            ret.push_back(0);
        }

        uint32_t new_stable_end = ret.size();

        uint32_t extra = (p_manifest.size() - string_table_ends);
        ret.resize(new_stable_end + extra);
        for (uint32_t i = 0; i < extra; i++) {
            ret.write[new_stable_end + i] = p_manifest[string_table_ends + i];
        }

        while (ret.size() % 4) {
            ret.push_back(0);
        }
        encode_uint32(ret.size(), &ret.write[4]); //update new file size

        encode_uint32(new_stable_end - 8, &ret.write[12]); //update new string table size
        encode_uint32(string_table.size(), &ret.write[16]); //update new number of strings
        encode_uint32(string_data_offset - 8, &ret.write[28]); //update new string data offset

        p_manifest = ret;
    }

    static String _parse_string(const uint8_t *p_bytes, bool p_utf8) {
        uint32_t offset = 0;
        uint32_t len = 0;

        if (p_utf8) {
            uint8_t byte = p_bytes[offset];
            if (byte & 0x80) {
                offset += 2;
            } else {
                offset += 1;
            }
            byte = p_bytes[offset];
            offset++;
            if (byte & 0x80) {
                len = byte & 0x7F;
                len = (len << 8) + p_bytes[offset];
                offset++;
            } else {
                len = byte;
            }
        } else {
            len = decode_uint16(&p_bytes[offset]);
            offset += 2;
            if (len & 0x8000) {
                len &= 0x7FFF;
                len = (len << 16) + decode_uint16(&p_bytes[offset]);
                offset += 2;
            }
        }

        if (p_utf8) {
            Vector<uint8_t> str8;
            str8.resize(len + 1);
            for (uint32_t i = 0; i < len; i++) {
                str8.write[i] = p_bytes[offset + i];
            }
            str8.write[len] = 0;
            String str;
            str.parse_utf8((const char *)str8.ptr());
            return str;
        } else {
            String str;
            for (uint32_t i = 0; i < len; i++) {
                char32_t c = decode_uint16(&p_bytes[offset + i * 2]);
                if (c == 0) {
                    break;
                }
                str += String::chr(c);
            }
            return str;
        }
    }

    void _fix_resources(const Ref<EditorExportPreset> &p_preset, Vector<uint8_t> &r_manifest) {
        const int UTF8_FLAG = 0x00000100;

        uint32_t string_block_len = decode_uint32(&r_manifest[16]);
        uint32_t string_count = decode_uint32(&r_manifest[20]);
        uint32_t string_flags = decode_uint32(&r_manifest[28]);
        const uint32_t string_table_begins = 40;

        Vector<String> string_table;

        String package_name = p_preset->get("package/name");

        for (uint32_t i = 0; i < string_count; i++) {
            uint32_t offset = decode_uint32(&r_manifest[string_table_begins + i * 4]);
            offset += string_table_begins + string_count * 4;

            String str = _parse_string(&r_manifest[offset], string_flags & UTF8_FLAG);

            if (str.begins_with("godot-project-name")) {
                if (str == "godot-project-name") {
                    //project name
                    str = get_project_name(package_name);

                } else {
                    String lang = str.substr(str.rfind("-") + 1, str.length()).replace("-", "_");
                    String prop = "application/config/name_" + lang;
                    if (ProjectSettings::get_singleton()->has_setting(prop)) {
                        str = ProjectSettings::get_singleton()->get(prop);
                    } else {
                        str = get_project_name(package_name);
                    }
                }
            }

            string_table.push_back(str);
        }

        //write a new string table, but use 16 bits
        Vector<uint8_t> ret;
        ret.resize(string_table_begins + string_table.size() * 4);

        for (uint32_t i = 0; i < string_table_begins; i++) {
            ret.write[i] = r_manifest[i];
        }

        int ofs = 0;
        for (int i = 0; i < string_table.size(); i++) {
            encode_uint32(ofs, &ret.write[string_table_begins + i * 4]);
            ofs += string_table[i].length() * 2 + 2 + 2;
        }

        ret.resize(ret.size() + ofs);
        uint8_t *chars = &ret.write[ret.size() - ofs];
        for (int i = 0; i < string_table.size(); i++) {
            String s = string_table[i];
            encode_uint16(s.length(), chars);
            chars += 2;
            for (int j = 0; j < s.length(); j++) {
                encode_uint16(s[j], chars);
                chars += 2;
            }
            encode_uint16(0, chars);
            chars += 2;
        }

        //pad
        while (ret.size() % 4) {
            ret.push_back(0);
        }

        //change flags to not use utf8
        encode_uint32(string_flags & ~0x100, &ret.write[28]);
        //change length
        encode_uint32(ret.size() - 12, &ret.write[16]);
        //append the rest...
        int rest_from = 12 + string_block_len;
        int rest_to = ret.size();
        int rest_len = (r_manifest.size() - rest_from);
        ret.resize(ret.size() + (r_manifest.size() - rest_from));
        for (int i = 0; i < rest_len; i++) {
            ret.write[rest_to + i] = r_manifest[rest_from + i];
        }
        //finally update the size
        encode_uint32(ret.size(), &ret.write[4]);

        r_manifest = ret;
        //printf("end\n");
    }

    void _load_image_data(const Ref<Image> &p_splash_image, Vector<uint8_t> &p_data) {
        Vector<uint8_t> png_buffer;
        Error err = PNGDriverCommon::image_to_png(p_splash_image, png_buffer);
        if (err == OK) {
            p_data.resize(png_buffer.size());
            memcpy(p_data.ptrw(), png_buffer.ptr(), p_data.size());
        } else {
            String err_str = String("Failed to convert splash image to png.");
            WARN_PRINT(err_str.utf8().get_data());
        }
    }

    void _process_launcher_icons(const String &p_file_name, const Ref<Image> &p_source_image, int dimension, Vector<uint8_t> &p_data) {
        Ref<Image> working_image = p_source_image;

        if (p_source_image->get_width() != dimension || p_source_image->get_height() != dimension) {
            working_image = p_source_image->duplicate();
            working_image->resize(dimension, dimension, Image::Interpolation::INTERPOLATE_LANCZOS);
        }

        Vector<uint8_t> png_buffer;
        Error err = PNGDriverCommon::image_to_png(working_image, png_buffer);
        if (err == OK) {
            p_data.resize(png_buffer.size());
            memcpy(p_data.ptrw(), png_buffer.ptr(), p_data.size());
        } else {
            String err_str = String("Failed to convert resized icon (") + p_file_name + ") to png.";
            WARN_PRINT(err_str.utf8().get_data());
        }
    }

    String load_splash_refs(Ref<Image> &splash_image, Ref<Image> &splash_bg_color_image) {
        bool scale_splash = ProjectSettings::get_singleton()->get("application/boot_splash/fullsize");
        bool apply_filter = ProjectSettings::get_singleton()->get("application/boot_splash/use_filter");
        String project_splash_path = ProjectSettings::get_singleton()->get("application/boot_splash/image");

        if (!project_splash_path.is_empty()) {
            splash_image.instance();
            print_verbose("Loading splash image: " + project_splash_path);
            const Error err = ImageLoader::load_image(project_splash_path, splash_image);
            if (err) {
                if (OS::get_singleton()->is_stdout_verbose()) {
                    print_error("- unable to load splash image from " + project_splash_path + " (" + itos(err) + ")");
                }
                splash_image.unref();
            }
        }

        if (splash_image.is_null()) {
            // Use the default
            print_verbose("Using default splash image.");
            splash_image = Ref<Image>(memnew(Image(boot_splash_png)));
        }

        if (scale_splash) {
            Size2 screen_size = Size2(ProjectSettings::get_singleton()->get("display/window/size/width"), ProjectSettings::get_singleton()->get("display/window/size/height"));
            int width, height;
            if (screen_size.width > screen_size.height) {
                // scale horizontally
                height = screen_size.height;
                width = splash_image->get_width() * screen_size.height / splash_image->get_height();
            } else {
                // scale vertically
                width = screen_size.width;
                height = splash_image->get_height() * screen_size.width / splash_image->get_width();
            }
            splash_image->resize(width, height);
        }

        // Setup the splash bg color
        bool bg_color_valid;
        Color bg_color = ProjectSettings::get_singleton()->get("application/boot_splash/bg_color", &bg_color_valid);
        if (!bg_color_valid) {
            bg_color = boot_splash_bg_color;
        }

        print_verbose("Creating splash background color image.");
        splash_bg_color_image.instance();
        splash_bg_color_image->create(splash_image->get_width(), splash_image->get_height(), false, splash_image->get_format());
        splash_bg_color_image->fill(bg_color);

        String processed_splash_config_xml = vformat(SPLASH_CONFIG_XML_CONTENT, bool_to_string(apply_filter));
        return processed_splash_config_xml;
    }

    void load_icon_refs(const Ref<EditorExportPreset> &p_preset, Ref<Image> &icon, Ref<Image> &foreground, Ref<Image> &background) {
        String project_icon_path = ProjectSettings::get_singleton()->get("application/config/icon");

        icon.instance();
        foreground.instance();
        background.instance();

        // Regular icon: user selection -> project icon -> default.
        String path = static_cast<String>(p_preset->get(launcher_icon_option)).strip_edges();
        print_verbose("Loading regular icon from " + path);
        if (path.is_empty() || ImageLoader::load_image(path, icon) != OK) {
            print_verbose("- falling back to project icon: " + project_icon_path);
            ImageLoader::load_image(project_icon_path, icon);
        }

        // Adaptive foreground: user selection -> regular icon (user selection -> project icon -> default).
        path = static_cast<String>(p_preset->get(launcher_adaptive_icon_foreground_option)).strip_edges();
        print_verbose("Loading adaptive foreground icon from " + path);
        if (path.is_empty() || ImageLoader::load_image(path, foreground) != OK) {
            print_verbose("- falling back to using the regular icon");
            foreground = icon;
        }

        // Adaptive background: user selection -> default.
        path = static_cast<String>(p_preset->get(launcher_adaptive_icon_background_option)).strip_edges();
        if (!path.is_empty()) {
            print_verbose("Loading adaptive background icon from " + path);
            ImageLoader::load_image(path, background);
        }
    }

    void store_image(const LauncherIcon launcher_icon, const Vector<uint8_t> &data) {
        store_image(launcher_icon.export_path, data);
    }

    void store_image(const String &export_path, const Vector<uint8_t> &data) {
        String img_path = export_path.insert(0, "res://android/build/");
        store_file_at_path(img_path, data);
    }

    void _copy_icons_to_gradle_project(const Ref<EditorExportPreset> &p_preset,
            const String &processed_splash_config_xml,
            const Ref<Image> &splash_image,
            const Ref<Image> &splash_bg_color_image,
            const Ref<Image> &main_image,
            const Ref<Image> &foreground,
            const Ref<Image> &background) {
        // Store the splash configuration
        if (!processed_splash_config_xml.is_empty()) {
            print_verbose("Storing processed splash configuration: " + String("\n") + processed_splash_config_xml);
            store_string_at_path(SPLASH_CONFIG_PATH, processed_splash_config_xml);
        }

        // Store the splash image
        if (splash_image.is_valid() && !splash_image->is_empty()) {
            print_verbose("Storing splash image in " + String(SPLASH_IMAGE_EXPORT_PATH));
            Vector<uint8_t> data;
            _load_image_data(splash_image, data);
            store_image(SPLASH_IMAGE_EXPORT_PATH, data);
        }

        // Store the splash bg color image
        if (splash_bg_color_image.is_valid() && !splash_bg_color_image->is_empty()) {
            print_verbose("Storing splash background image in " + String(SPLASH_BG_COLOR_PATH));
            Vector<uint8_t> data;
            _load_image_data(splash_bg_color_image, data);
            store_image(SPLASH_BG_COLOR_PATH, data);
        }

        // Prepare images to be resized for the icons. If some image ends up being uninitialized,
        // the default image from the export template will be used.

        for (int i = 0; i < icon_densities_count; ++i) {
            if (main_image.is_valid() && !main_image->is_empty()) {
                print_verbose("Processing launcher icon for dimension " + itos(launcher_icons[i].dimensions) + " into " + launcher_icons[i].export_path);
                Vector<uint8_t> data;
                _process_launcher_icons(launcher_icons[i].export_path, main_image, launcher_icons[i].dimensions, data);
                store_image(launcher_icons[i], data);
            }

            if (foreground.is_valid() && !foreground->is_empty()) {
                print_verbose("Processing launcher adaptive icon foreground for dimension " + itos(launcher_adaptive_icon_foregrounds[i].dimensions) + " into " + launcher_adaptive_icon_foregrounds[i].export_path);
                Vector<uint8_t> data;
                _process_launcher_icons(launcher_adaptive_icon_foregrounds[i].export_path, foreground,
                        launcher_adaptive_icon_foregrounds[i].dimensions, data);
                store_image(launcher_adaptive_icon_foregrounds[i], data);
            }

            if (background.is_valid() && !background->is_empty()) {
                print_verbose("Processing launcher adaptive icon background for dimension " + itos(launcher_adaptive_icon_backgrounds[i].dimensions) + " into " + launcher_adaptive_icon_backgrounds[i].export_path);
                Vector<uint8_t> data;
                _process_launcher_icons(launcher_adaptive_icon_backgrounds[i].export_path, background,
                        launcher_adaptive_icon_backgrounds[i].dimensions, data);
                store_image(launcher_adaptive_icon_backgrounds[i], data);
            }
        }
    }

    static Vector<String> get_enabled_abis(const Ref<EditorExportPreset> &p_preset) {
        Vector<String> abis = get_abis();
        Vector<String> enabled_abis;
        for (int i = 0; i < abis.size(); ++i) {
            bool is_enabled = p_preset->get("architectures/" + abis[i]);
            if (is_enabled) {
                enabled_abis.push_back(abis[i]);
            }
        }
        return enabled_abis;
    }

public:
    typedef Error (*EditorExportSaveFunction)(void *p_userdata, const String &p_path, const Vector<uint8_t> &p_data, int p_file, int p_total, const Vector<String> &p_enc_in_filters, const Vector<String> &p_enc_ex_filters, const Vector<uint8_t> &p_key);

public:
    virtual void get_preset_features(const Ref<EditorExportPreset> &p_preset, List<String> *r_features) override;

    virtual void get_export_options(List<ExportOption> *r_options) override;

    virtual String get_name() const override;

    virtual String get_os_name() const override;

    virtual Ref<Texture2D> get_logo() const override;

    virtual bool should_update_export_options() override;

    virtual bool poll_export() override;

    virtual int get_options_count() const override;

    virtual String get_options_tooltip() const override;

    virtual String get_option_label(int p_index) const override;

    virtual String get_option_tooltip(int p_index) const override;

    virtual Error run(const Ref<EditorExportPreset> &p_preset, int p_device, int p_debug_flags) override;

    virtual Ref<Texture2D> get_run_icon() const override;

    static String get_adb_path();

    static String get_apksigner_path();

    virtual bool can_export(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates) const override;

    virtual List<String> get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const override;

    inline bool is_clean_build_required(Vector<PluginConfigAndroid> enabled_plugins);

    String get_apk_expansion_fullpath(const Ref<EditorExportPreset> &p_preset, const String &p_path);

    Error save_apk_expansion_file(const Ref<EditorExportPreset> &p_preset, const String &p_path);

    void get_command_line_flags(const Ref<EditorExportPreset> &p_preset, const String &p_path, int p_flags, Vector<uint8_t> &r_command_line_flags);

    Error sign_apk(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &export_path, EditorProgress &ep);

    void _clear_assets_directory();

    String join_list(List<String> parts, const String &separator) const;

    virtual Error export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, int p_flags = 0) override;

    Error export_project_helper(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, int export_format, bool should_sign, int p_flags);

    virtual void get_platform_features(List<String> *r_features) override;

    virtual void resolve_platform_feature_priorities(const Ref<EditorExportPreset> &p_preset, Set<String> &p_features) override;

    EditorExportPlatformAndroidAPK();

    ~EditorExportPlatformAndroidAPK();
};

#endif
