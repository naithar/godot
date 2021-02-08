/*************************************************************************/
/*  display_server_appletv.mm                                            */
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

#include "display_server_appletv.h"
#import "app_delegate.h"
#include "core/config/project_settings.h"
#include "core/io/file_access_pack.h"
#import "godot_view.h"
#import "keyboard_input_view.h"
#import "native_video_view.h"
#include "os_appletv.h"
#include "tvos.h"
#import "view_controller.h"

#import <Foundation/Foundation.h>
#import <sys/utsname.h>

DisplayServerAppleTV *DisplayServerAppleTV::get_singleton() {
	return (DisplayServerAppleTV *)DisplayServer::get_singleton();
}

DisplayServerAppleTV::DisplayServerAppleTV(const String &p_rendering_driver, DisplayServer::WindowMode p_mode, uint32_t p_flags, const Vector2i &p_resolution, Error &r_error) {
	rendering_driver = p_rendering_driver;

#if defined(OPENGL_ENABLED)
	// FIXME: Add support for both GLES2 and Vulkan when GLES2 is implemented
	// again,

	if (rendering_driver == "opengl_es") {
		bool gl_initialization_error = false;

		// FIXME: Add Vulkan support via MoltenVK. Add fallback code back?

		if (RasterizerGLES2::is_viable() == OK) {
			RasterizerGLES2::register_config();
			RasterizerGLES2::make_current();
		} else {
			gl_initialization_error = true;
		}

		if (gl_initialization_error) {
			OS::get_singleton()->alert("Your device does not support any of the supported OpenGL versions.", "Unable to initialize video driver");
			//        return ERR_UNAVAILABLE;
		}

		//    rendering_server = memnew(RenderingServerDefault);
		//    // FIXME: Reimplement threaded rendering
		//    if (get_render_thread_mode() != RENDER_THREAD_UNSAFE) {
		//        rendering_server = memnew(RenderingServerWrapMT(rendering_server,
		//        false));
		//    }
		//    rendering_server->init();
		// rendering_server->cursor_set_visible(false, 0);

		// reset this to what it should be, it will have been set to 0 after
		// rendering_server->init() is called
		//    RasterizerStorageGLES2::system_fbo = gl_view_base_fb;
	}
#endif

#if defined(VULKAN_ENABLED)
	rendering_driver = "vulkan";

	context_vulkan = nullptr;
	rendering_device_vulkan = nullptr;

	if (rendering_driver == "vulkan") {
		context_vulkan = memnew(VulkanContextAppleTV);
		if (context_vulkan->initialize() != OK) {
			memdelete(context_vulkan);
			context_vulkan = nullptr;
			ERR_FAIL_MSG("Failed to initialize Vulkan context");
		}

		CALayer *layer = [AppDelegate.viewController.godotView initializeRenderingForDriver:@"vulkan"];

		if (!layer) {
			ERR_FAIL_MSG("Failed to create iOS rendering layer.");
		}

		Size2i size = Size2i(layer.bounds.size.width, layer.bounds.size.height) * screen_get_max_scale();
		if (context_vulkan->window_create(MAIN_WINDOW_ID, layer, size.width, size.height) != OK) {
			memdelete(context_vulkan);
			context_vulkan = nullptr;
			ERR_FAIL_MSG("Failed to create Vulkan window.");
		}

		rendering_device_vulkan = memnew(RenderingDeviceVulkan);
		rendering_device_vulkan->initialize(context_vulkan);

		RendererCompositorRD::make_current();
	}
#endif

	bool keep_screen_on = bool(GLOBAL_DEF("display/window/energy_saving/keep_screen_on", true));
	screen_set_keep_on(keep_screen_on);

	Input::get_singleton()->set_event_dispatch_function(_dispatch_input_events);

	r_error = OK;
}

DisplayServerAppleTV::~DisplayServerAppleTV() {
#if defined(VULKAN_ENABLED)
	if (rendering_driver == "vulkan") {
		if (rendering_device_vulkan) {
			rendering_device_vulkan->finalize();
			memdelete(rendering_device_vulkan);
			rendering_device_vulkan = NULL;
		}

		if (context_vulkan) {
			context_vulkan->window_destroy(MAIN_WINDOW_ID);
			memdelete(context_vulkan);
			context_vulkan = NULL;
		}
	}
#endif
}

DisplayServer *DisplayServerAppleTV::create_func(const String &p_rendering_driver, WindowMode p_mode, uint32_t p_flags, const Vector2i &p_resolution, Error &r_error) {
	return memnew(DisplayServerAppleTV(p_rendering_driver, p_mode, p_flags, p_resolution, r_error));
}

Vector<String> DisplayServerAppleTV::get_rendering_drivers_func() {
	Vector<String> drivers;

#if defined(VULKAN_ENABLED)
	drivers.push_back("vulkan");
#endif
#if defined(OPENGL_ENABLED)
	drivers.push_back("opengl_es");
#endif

	return drivers;
}

void DisplayServerAppleTV::register_appletv_driver() {
	register_create_function("tvos", create_func, get_rendering_drivers_func);
}

// MARK: Events

void DisplayServerAppleTV::window_set_rect_changed_callback(const Callable &p_callable, WindowID p_window) {
	window_resize_callback = p_callable;
}

void DisplayServerAppleTV::window_set_window_event_callback(const Callable &p_callable, WindowID p_window) {
	window_event_callback = p_callable;
}
void DisplayServerAppleTV::window_set_input_event_callback(const Callable &p_callable, WindowID p_window) {
	input_event_callback = p_callable;
}

void DisplayServerAppleTV::window_set_input_text_callback(const Callable &p_callable, WindowID p_window) {
	input_text_callback = p_callable;
}

void DisplayServerAppleTV::window_set_drop_files_callback(const Callable &p_callable, WindowID p_window) {
	// Probably not supported for iOS
}

void DisplayServerAppleTV::process_events() {
}

void DisplayServerAppleTV::_dispatch_input_events(const Ref<InputEvent> &p_event) {
	DisplayServerAppleTV::get_singleton()->send_input_event(p_event);
}

void DisplayServerAppleTV::send_input_event(const Ref<InputEvent> &p_event) const {
	_window_callback(input_event_callback, p_event);
}

void DisplayServerAppleTV::send_input_text(const String &p_text) const {
	_window_callback(input_text_callback, p_text);
}

void DisplayServerAppleTV::send_window_event(DisplayServer::WindowEvent p_event) const {
	_window_callback(window_event_callback, int(p_event));
}

void DisplayServerAppleTV::_window_callback(const Callable &p_callable, const Variant &p_arg) const {
	if (!p_callable.is_null()) {
		const Variant *argp = &p_arg;
		Variant ret;
		Callable::CallError ce;
		p_callable.call((const Variant **)&argp, 1, ret, ce);
	}
}

// MARK: - Input

void DisplayServerAppleTV::perform_event(const Ref<InputEvent> &p_event) {
	Input::get_singleton()->parse_input_event(p_event);
}

// MARK: Keyboard

void DisplayServerAppleTV::key(uint32_t p_key, bool p_pressed) {
	Ref<InputEventKey> ev;
	ev.instance();
	ev->set_echo(false);
	ev->set_pressed(p_pressed);
	ev->set_keycode(p_key);
	ev->set_physical_keycode(p_key);
	ev->set_unicode(p_key);
	perform_event(ev);
};

// MARK: -

bool DisplayServerAppleTV::has_feature(Feature p_feature) const {
	switch (p_feature) {
			// case FEATURE_CONSOLE_WINDOW:
			// case FEATURE_CURSOR_SHAPE:
			// case FEATURE_CUSTOM_CURSOR_SHAPE:
			// case FEATURE_GLOBAL_MENU:
			// case FEATURE_HIDPI:
			// case FEATURE_ICON:
			// case FEATURE_IME:
			// case FEATURE_MOUSE:
			// case FEATURE_MOUSE_WARP:
			// case FEATURE_NATIVE_DIALOG:
			// case FEATURE_NATIVE_ICON:
			// case FEATURE_NATIVE_VIDEO:
			// case FEATURE_WINDOW_TRANSPARENCY:
			//		case FEATURE_CLIPBOARD:
		case FEATURE_KEEP_SCREEN_ON:
			//		case FEATURE_ORIENTATION:
		case FEATURE_TOUCHSCREEN:
		case FEATURE_VIRTUAL_KEYBOARD:
			return true;
		default:
			return false;
	}
}

String DisplayServerAppleTV::get_name() const {
	return "tvOS";
}

void DisplayServerAppleTV::alert(const String &p_alert, const String &p_title) {
	const CharString utf8_alert = p_alert.utf8();
	const CharString utf8_title = p_title.utf8();
	tvOS::alert(utf8_alert.get_data(), utf8_title.get_data());
}

int DisplayServerAppleTV::get_screen_count() const {
	return 1;
}

Point2i DisplayServerAppleTV::screen_get_position(int p_screen) const {
	return Size2i();
}

Size2i DisplayServerAppleTV::screen_get_size(int p_screen) const {
	CALayer *layer = AppDelegate.viewController.godotView.renderingLayer;

	if (!layer) {
		return Size2i();
	}

	return Size2i(layer.bounds.size.width, layer.bounds.size.height) * screen_get_scale(p_screen);
}

Rect2i DisplayServerAppleTV::screen_get_usable_rect(int p_screen) const {
	if (@available(tvOS 11, *)) {
		UIEdgeInsets insets = UIEdgeInsetsZero;
		UIView *view = AppDelegate.viewController.godotView;

		if ([view respondsToSelector:@selector(safeAreaInsets)]) {
			insets = [view safeAreaInsets];
		}

		float scale = screen_get_scale(p_screen);
		Size2i insets_position = Size2i(insets.left, insets.top) * scale;
		Size2i insets_size = Size2i(insets.left + insets.right, insets.top + insets.bottom) * scale;

		return Rect2i(screen_get_position(p_screen) + insets_position, screen_get_size(p_screen) - insets_size);
	} else {
		return Rect2i(screen_get_position(p_screen), screen_get_size(p_screen));
	}
}

int DisplayServerAppleTV::screen_get_dpi(int p_screen) const {
	return 96;
}

float DisplayServerAppleTV::screen_get_scale(int p_screen) const {
	return [UIScreen mainScreen].nativeScale;
}

Vector<DisplayServer::WindowID> DisplayServerAppleTV::get_window_list() const {
	Vector<DisplayServer::WindowID> list;
	list.push_back(MAIN_WINDOW_ID);
	return list;
}

DisplayServer::WindowID DisplayServerAppleTV::get_window_at_screen_position(const Point2i &p_position) const {
	return MAIN_WINDOW_ID;
}

void DisplayServerAppleTV::window_attach_instance_id(ObjectID p_instance, WindowID p_window) {
	window_attached_instance_id = p_instance;
}

ObjectID DisplayServerAppleTV::window_get_attached_instance_id(WindowID p_window) const {
	return window_attached_instance_id;
}

void DisplayServerAppleTV::window_set_title(const String &p_title, WindowID p_window) {
	// Probably not supported for iOS
}

int DisplayServerAppleTV::window_get_current_screen(WindowID p_window) const {
	return SCREEN_OF_MAIN_WINDOW;
}

void DisplayServerAppleTV::window_set_current_screen(int p_screen, WindowID p_window) {
	// Probably not supported for iOS
}

Point2i DisplayServerAppleTV::window_get_position(WindowID p_window) const {
	return Point2i();
}

void DisplayServerAppleTV::window_set_position(const Point2i &p_position, WindowID p_window) {
	// Probably not supported for single window iOS app
}

void DisplayServerAppleTV::window_set_transient(WindowID p_window, WindowID p_parent) {
	// Probably not supported for iOS
}

void DisplayServerAppleTV::window_set_max_size(const Size2i p_size, WindowID p_window) {
	// Probably not supported for iOS
}

Size2i DisplayServerAppleTV::window_get_max_size(WindowID p_window) const {
	return Size2i();
}

void DisplayServerAppleTV::window_set_min_size(const Size2i p_size, WindowID p_window) {
	// Probably not supported for iOS
}

Size2i DisplayServerAppleTV::window_get_min_size(WindowID p_window) const {
	return Size2i();
}

void DisplayServerAppleTV::window_set_size(const Size2i p_size, WindowID p_window) {
	// Probably not supported for iOS
}

Size2i DisplayServerAppleTV::window_get_size(WindowID p_window) const {
	CGRect screenBounds = [UIScreen mainScreen].bounds;
	return Size2i(screenBounds.size.width, screenBounds.size.height) * screen_get_max_scale();
}

Size2i DisplayServerAppleTV::window_get_real_size(WindowID p_window) const {
	return window_get_size(p_window);
}

void DisplayServerAppleTV::window_set_mode(WindowMode p_mode, WindowID p_window) {
	// Probably not supported for iOS
}

DisplayServer::WindowMode DisplayServerAppleTV::window_get_mode(WindowID p_window) const {
	return WindowMode::WINDOW_MODE_FULLSCREEN;
}

bool DisplayServerAppleTV::window_is_maximize_allowed(WindowID p_window) const {
	return false;
}

void DisplayServerAppleTV::window_set_flag(WindowFlags p_flag, bool p_enabled, WindowID p_window) {
	// Probably not supported for iOS
}

bool DisplayServerAppleTV::window_get_flag(WindowFlags p_flag, WindowID p_window) const {
	return false;
}

void DisplayServerAppleTV::window_request_attention(WindowID p_window) {
	// Probably not supported for iOS
}

void DisplayServerAppleTV::window_move_to_foreground(WindowID p_window) {
	// Probably not supported for iOS
}

float DisplayServerAppleTV::screen_get_max_scale() const {
	return screen_get_scale(SCREEN_OF_MAIN_WINDOW);
};

void DisplayServerAppleTV::screen_set_orientation(DisplayServer::ScreenOrientation p_orientation, int p_screen) {
}

DisplayServer::ScreenOrientation DisplayServerAppleTV::screen_get_orientation(int p_screen) const {
	return SCREEN_LANDSCAPE;
}

bool DisplayServerAppleTV::window_can_draw(WindowID p_window) const {
	return true;
}

bool DisplayServerAppleTV::can_any_window_draw() const {
	return true;
}

bool DisplayServerAppleTV::screen_is_touchscreen(int p_screen) const {
	return true;
}

void DisplayServerAppleTV::virtual_keyboard_show(const String &p_existing_text, const Rect2 &p_screen_rect, bool p_multiline, int p_max_length, int p_cursor_start, int p_cursor_end) {
	NSString *existingString = [[NSString alloc] initWithUTF8String:p_existing_text.utf8().get_data()];

	[AppDelegate.viewController.keyboardView
			becomeFirstResponderWithString:existingString
								 multiline:p_multiline
							   cursorStart:p_cursor_start
								 cursorEnd:p_cursor_end];
}

void DisplayServerAppleTV::virtual_keyboard_hide() {
	[AppDelegate.viewController.keyboardView resignFirstResponder];
}

int DisplayServerAppleTV::virtual_keyboard_get_height() const {
	return 0;
}

void DisplayServerAppleTV::clipboard_set(const String &p_text) {
}

String DisplayServerAppleTV::clipboard_get() const {
	return "";
}

void DisplayServerAppleTV::screen_set_keep_on(bool p_enable) {
	[UIApplication sharedApplication].idleTimerDisabled = p_enable;
}

bool DisplayServerAppleTV::screen_is_kept_on() const {
	return [UIApplication sharedApplication].idleTimerDisabled;
}

Error DisplayServerAppleTV::native_video_play(String p_path, float p_volume, String p_audio_track, String p_subtitle_track, int p_screen) {
	FileAccess *f = FileAccess::open(p_path, FileAccess::READ);
	bool exists = f && f->is_open();

	String user_data_dir = OSAppleTV::get_singleton()->get_user_data_dir();

	if (!exists) {
		return FAILED;
	}

	String tempFile = OSAppleTV::get_singleton()->get_user_data_dir();

	if (p_path.begins_with("res://")) {
		if (PackedData::get_singleton()->has_path(p_path)) {
			printf("Unable to play %s using the native player as it resides in a .pck file\n", p_path.utf8().get_data());
			return ERR_INVALID_PARAMETER;
		} else {
			p_path = p_path.replace("res:/", ProjectSettings::get_singleton()->get_resource_path());
		}
	} else if (p_path.begins_with("user://")) {
		p_path = p_path.replace("user:/", user_data_dir);
	}

	memdelete(f);

	printf("Playing video: %s\n", p_path.utf8().get_data());

	String file_path = ProjectSettings::get_singleton()->globalize_path(p_path);

	NSString *filePath = [[NSString alloc] initWithUTF8String:file_path.utf8().get_data()];
	NSString *audioTrack = [NSString stringWithUTF8String:p_audio_track.utf8()];
	NSString *subtitleTrack = [NSString stringWithUTF8String:p_subtitle_track.utf8()];

	if (![AppDelegate.viewController playVideoAtPath:filePath
											  volume:p_volume
											   audio:audioTrack
											subtitle:subtitleTrack]) {
		return OK;
	}

	return FAILED;
}

bool DisplayServerAppleTV::native_video_is_playing() const {
	return [AppDelegate.viewController.videoView isVideoPlaying];
}

void DisplayServerAppleTV::native_video_pause() {
	if (native_video_is_playing()) {
		[AppDelegate.viewController.videoView pauseVideo];
	}
}

void DisplayServerAppleTV::native_video_unpause() {
	[AppDelegate.viewController.videoView unpauseVideo];
};

void DisplayServerAppleTV::native_video_stop() {
	if (native_video_is_playing()) {
		[AppDelegate.viewController.videoView stopVideo];
	}
}

void DisplayServerAppleTV::resize_window(CGSize viewSize) {
	Size2i size = Size2i(viewSize.width, viewSize.height) * screen_get_max_scale();

#if defined(VULKAN_ENABLED)
	if (rendering_driver == "vulkan") {
		if (context_vulkan) {
			context_vulkan->window_resize(MAIN_WINDOW_ID, size.x, size.y);
		}
	}
#endif

	Variant resize_rect = Rect2i(Point2i(), size);
	_window_callback(window_resize_callback, resize_rect);
}
