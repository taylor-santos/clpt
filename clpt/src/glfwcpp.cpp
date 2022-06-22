//
// Created by taylor-santos on 6/14/2022 at 09:56.
//

#include <GLFW/glfw3.h>
#include <stdexcept>
#include <iostream>

#include "glfwcpp.hpp"

#define MAIN_THREAD_ONLY()                                                                      \
    do {                                                                                        \
        if (main_thread) {                                                                      \
            if (std::this_thread::get_id() != main_thread) {                                    \
                using namespace std::string_literals;                                           \
                throw std::logic_error(                                                         \
                    "GLFW error: tried calling "s + __FUNCTION__ + " from a non-main thread!"); \
            }                                                                                   \
        } else {                                                                                \
            throw std::logic_error("GLFW not initialized!");                                    \
        }                                                                                       \
    } while (0)

namespace GLFW {

static std::optional<std::thread::id> main_thread;

Error::Error(int code, const char *msg)
    : std::runtime_error(msg)
    , message_{"GLFW Error " + std::to_string(code) + ": " + msg} {}

const char *
Error::what() const noexcept {
    return message_.c_str();
}

Init::Init() {
    glfwSetErrorCallback([](int code, const char *msg) { throw Error(code, msg); });
    if (!glfwInit()) throw std::runtime_error("Failed to initialize GLFW");
    main_thread = std::this_thread::get_id();
}

Init::~Init() {
    glfwTerminate();
}

Monitor::Monitor(GLFWmonitor *monitor)
    : monitor_{monitor} {
    if (!monitor_) {
        const char *msg;
        int         error = glfwGetError(&msg);
        if (error == GLFW_NO_ERROR) {
            throw std::invalid_argument("NULL provided as GLFW Monitor pointer");
        } else {
            throw Error(error, msg);
        }
    }
    glfwSetMonitorUserPointer(monitor_, this);
}

std::vector<Monitor>
Monitor::get_all() {
    MAIN_THREAD_ONLY();
    int                  count;
    auto                *monitors = glfwGetMonitors(&count);
    std::vector<Monitor> all_monitors;
    all_monitors.reserve(count);
    for (int i = 0; i < count; i++) {
        all_monitors.push_back(monitors[i]);
    }
    return all_monitors;
}

Monitor
Monitor::get_primary() {
    MAIN_THREAD_ONLY();
    return glfwGetPrimaryMonitor();
}

std::tuple<int, int>
Monitor::get_pos() const {
    MAIN_THREAD_ONLY();
    int x, y;
    glfwGetMonitorPos(monitor_, &x, &y);
    return {x, y};
}

std::tuple<int, int, int, int>
Monitor::get_workarea() const {
    MAIN_THREAD_ONLY();
    int x, y, w, h;
    glfwGetMonitorWorkarea(monitor_, &x, &y, &w, &h);
    return {x, y, w, h};
}

std::tuple<int, int>
Monitor::get_physical_size() const {
    MAIN_THREAD_ONLY();
    int w, h;
    glfwGetMonitorPhysicalSize(monitor_, &w, &h);
    return {w, h};
}

std::tuple<float, float>
Monitor::get_content_scale() const {
    MAIN_THREAD_ONLY();
    float x, y;
    glfwGetMonitorContentScale(monitor_, &x, &y);
    return {x, y};
}

std::string
Monitor::get_name() const {
    MAIN_THREAD_ONLY();
    return glfwGetMonitorName(monitor_);
}

Monitor::Fun
Monitor::set_callback(const Monitor::Fun &callback) {
    MAIN_THREAD_ONLY();
    return std::exchange(callback_, callback);
}

std::vector<Vidmode>
Monitor::get_video_modes() const {
    MAIN_THREAD_ONLY();
    int                  count;
    auto                *vidmodes = glfwGetVideoModes(monitor_, &count);
    std::vector<Vidmode> all_vidmodes;
    all_vidmodes.reserve(count);
    for (int i = 0; i < count; i++) {
        auto [w, h, r, g, b, rr] = vidmodes[i];
        all_vidmodes.push_back({w, h, r, g, b, rr});
    }
    return all_vidmodes;
}

Vidmode
Monitor::get_video_mode() const {
    MAIN_THREAD_ONLY();
    auto [w, h, r, g, b, rr] = *glfwGetVideoMode(monitor_);
    return {w, h, r, g, b, rr};
}

void
Monitor::set_gamma(float gamma) {
    MAIN_THREAD_ONLY();
    glfwSetGamma(monitor_, gamma);
}

std::vector<GammaRamp>
Monitor::get_gamma_ramp() const {
    MAIN_THREAD_ONLY();
    auto [red, green, blue, size] = *glfwGetGammaRamp(monitor_);
    std::vector<GammaRamp> ramp_vec;
    ramp_vec.reserve(size);
    for (unsigned int i = 0; i < size; i++) {
        ramp_vec.push_back({red[i], green[i], blue[i]});
    }
    return ramp_vec;
}

void
Monitor::set_gamma_ramp(const std::vector<GammaRamp> &gamma_ramp) {
    MAIN_THREAD_ONLY();
    auto size = static_cast<unsigned int>(gamma_ramp.size());

    std::vector<unsigned short> reds, greens, blues;
    reds.reserve(size);
    greens.reserve(size);
    blues.reserve(size);
    for (auto &gamma : gamma_ramp) {
        reds.push_back(gamma.red);
        greens.reserve(gamma.green);
        blues.reserve(gamma.blue);
    }
    GLFWgammaramp ramp{reds.data(), greens.data(), blues.data(), size};
    glfwSetGammaRamp(monitor_, &ramp);
}

GLFWmonitor *
Monitor::operator()() const {
    return monitor_;
}

Cursor::Cursor(Image &&image, int xhot, int yhot) {
    MAIN_THREAD_ONLY();
    auto img = GLFWimage{image.width, image.height, image.pixels.data()};
    cursor_  = glfwCreateCursor(&img, xhot, yhot);
}

Cursor::Cursor(CursorShape shape) {
    MAIN_THREAD_ONLY();
    cursor_ = glfwCreateStandardCursor(static_cast<int>(shape));
}

Cursor::~Cursor() {
    glfwDestroyCursor(cursor_);
}

GLFWcursor *
Cursor::operator()() const {
    return cursor_;
}

void
Window::hint(int hint, int value) {
    MAIN_THREAD_ONLY();
    glfwWindowHint(static_cast<int>(hint), value);
}

void
Window::hint(int hint, const std::string &str) {
    MAIN_THREAD_ONLY();
    glfwWindowHintString(static_cast<int>(hint), str.c_str());
}

void
Window::default_hints() {
    MAIN_THREAD_ONLY();
    glfwDefaultWindowHints();
}

const Window *
Window::get_current_context() {
    auto *wptr = glfwGetCurrentContext();
    if (!wptr) return nullptr;
    return static_cast<Window *>(glfwGetWindowUserPointer(wptr));
}

void
Window::clear_current_context() {
    auto curr_window = get_current_context();
    glfwMakeContextCurrent(nullptr);
    if (curr_window) {
        curr_window->current_thread_.reset();
    }
}

Window::Window(int width, int height, const char *title, GLFWmonitor *monitor, GLFWwindow *share) {
    MAIN_THREAD_ONLY();
    window_ = glfwCreateWindow(width, height, title, monitor, share);

    glfwSetWindowUserPointer(window_, this);

    glfwSetWindowPosCallback(window_, [](auto *wptr, int x, int y) {
        auto *window = static_cast<Window *>(glfwGetWindowUserPointer(wptr));
        if (window->pos_fun_) window->pos_fun_(x, y);
    });
    glfwSetWindowSizeCallback(window_, [](auto *wptr, int w, int h) {
        auto *window = static_cast<Window *>(glfwGetWindowUserPointer(wptr));
        if (window->size_fun_) window->size_fun_(w, h);
    });
    glfwSetWindowCloseCallback(window_, [](auto *wptr) {
        auto *window = static_cast<Window *>(glfwGetWindowUserPointer(wptr));
        if (window->close_fun_) window->close_fun_();
    });
    glfwSetWindowRefreshCallback(window_, [](auto *wptr) {
        auto *window = static_cast<Window *>(glfwGetWindowUserPointer(wptr));
        if (window->refresh_fun_) window->refresh_fun_();
    });
    glfwSetWindowFocusCallback(window_, [](auto *wptr, int focused) {
        auto *window = static_cast<Window *>(glfwGetWindowUserPointer(wptr));
        if (window->focus_fun_) window->focus_fun_(focused);
    });
    glfwSetWindowIconifyCallback(window_, [](auto *wptr, int iconified) {
        auto *window = static_cast<Window *>(glfwGetWindowUserPointer(wptr));
        if (window->iconify_fun_) window->iconify_fun_(iconified);
    });
    glfwSetWindowMaximizeCallback(window_, [](auto *wptr, int maximized) {
        auto *window = static_cast<Window *>(glfwGetWindowUserPointer(wptr));
        if (window->maximize_fun_) window->maximize_fun_(maximized);
    });
    glfwSetFramebufferSizeCallback(window_, [](auto *wptr, int width, int height) {
        auto *window = static_cast<Window *>(glfwGetWindowUserPointer(wptr));
        if (window->framebuffer_size_fun_) window->framebuffer_size_fun_(width, height);
    });
    glfwSetWindowContentScaleCallback(window_, [](auto *wptr, float x, float y) {
        auto *window = static_cast<Window *>(glfwGetWindowUserPointer(wptr));
        if (window->content_scale_fun_) window->content_scale_fun_(x, y);
    });

    glfwSetMouseButtonCallback(window_, [](auto *wptr, int button, int action, int mods) {
        auto *window = static_cast<Window *>(glfwGetWindowUserPointer(wptr));
        if (window->mouse_button_fun_)
            window->mouse_button_fun_(
                static_cast<Button>(button),
                static_cast<Action>(action),
                static_cast<Modifier>(mods));
    });
    glfwSetCursorPosCallback(window_, [](auto *wptr, double x, double y) {
        auto *window = static_cast<Window *>(glfwGetWindowUserPointer(wptr));
        if (window->cursor_pos_fun_) window->cursor_pos_fun_(x, y);
    });
    glfwSetCursorEnterCallback(window_, [](auto *wptr, int entered) {
        auto *window = static_cast<Window *>(glfwGetWindowUserPointer(wptr));
        if (window->cursor_enter_fun_) window->cursor_enter_fun_(entered);
    });
    glfwSetScrollCallback(window_, [](auto *wptr, double x, double y) {
        auto *window = static_cast<Window *>(glfwGetWindowUserPointer(wptr));
        if (window->scroll_fun_) window->scroll_fun_(x, y);
    });
    glfwSetKeyCallback(window_, [](auto *wptr, int key, int scancode, int action, int mods) {
        auto *window = static_cast<Window *>(glfwGetWindowUserPointer(wptr));

        auto opt = window->key_map_[key];
        if (opt) {
            opt->first(
                static_cast<Key>(key),
                scancode,
                static_cast<Action>(action),
                static_cast<Modifier>(mods));
            if (opt->second) {
                // Exclusive flag is set, return early
                return;
            }
        }

        if (window->key_fun_)
            window->key_fun_(
                static_cast<Key>(key),
                scancode,
                static_cast<Action>(action),
                static_cast<Modifier>(mods));
    });
    glfwSetCharCallback(window_, [](auto *wptr, unsigned int entered) {
        auto *window = static_cast<Window *>(glfwGetWindowUserPointer(wptr));
        if (window->char_fun_) window->char_fun_(entered);
    });
    glfwSetDropCallback(window_, [](auto *wptr, int path_count, const char *paths[]) {
        auto *window = static_cast<Window *>(glfwGetWindowUserPointer(wptr));
        if (window->drop_fun_) window->drop_fun_({paths, paths + path_count});
    });
}

Window::Window(int width, int height, const char *title, Monitor *monitor, Window *share)
    : Window(
          width,
          height,
          title,
          monitor ? monitor->monitor_ : nullptr,
          share ? share->window_ : nullptr) {}

Window::Window(Window &&other) noexcept
    : window_{std::exchange(other.window_, nullptr)}
    , current_thread_{std::exchange(other.current_thread_, {})}
    , pos_fun_{std::move(other.pos_fun_)}
    , size_fun_{std::move(other.size_fun_)}
    , close_fun_{std::move(other.close_fun_)}
    , refresh_fun_{std::move(other.refresh_fun_)}
    , focus_fun_{std::move(other.focus_fun_)}
    , iconify_fun_{std::move(other.iconify_fun_)}
    , maximize_fun_{std::move(other.maximize_fun_)}
    , framebuffer_size_fun_{std::move(other.framebuffer_size_fun_)}
    , content_scale_fun_{std::move(other.content_scale_fun_)}
    , mouse_button_fun_{std::move(other.mouse_button_fun_)}
    , cursor_pos_fun_{std::move(other.cursor_pos_fun_)}
    , cursor_enter_fun_{std::move(other.cursor_enter_fun_)}
    , scroll_fun_{std::move(other.scroll_fun_)}
    , key_fun_{std::move(other.key_fun_)}
    , char_fun_{std::move(other.char_fun_)}
    , drop_fun_{std::move(other.drop_fun_)} {}

Window &
Window::operator=(Window &&other) noexcept {
    std::swap(window_, other.window_);
    std::swap(current_thread_, other.current_thread_);

    pos_fun_              = other.pos_fun_;
    size_fun_             = other.size_fun_;
    close_fun_            = other.close_fun_;
    refresh_fun_          = other.refresh_fun_;
    focus_fun_            = other.focus_fun_;
    iconify_fun_          = other.iconify_fun_;
    maximize_fun_         = other.maximize_fun_;
    framebuffer_size_fun_ = other.framebuffer_size_fun_;
    content_scale_fun_    = other.content_scale_fun_;
    mouse_button_fun_     = other.mouse_button_fun_;
    cursor_pos_fun_       = other.cursor_pos_fun_;
    cursor_enter_fun_     = other.cursor_enter_fun_;
    scroll_fun_           = other.scroll_fun_;
    key_fun_              = other.key_fun_;
    char_fun_             = other.char_fun_;
    drop_fun_             = other.drop_fun_;

    return *this;
}

Window::~Window() {
    if (window_) {
        if (current_thread_ && std::this_thread::get_id() != current_thread_) {
            std::cerr
                << "Window is being destroyed while its context is current in a different thread"
                << std::endl;
        }
        glfwDestroyWindow(window_);
    }
}

GLFWwindow *
Window::operator()() const {
    return window_;
}

void
Window::make_context_current() const {
    glfwMakeContextCurrent(window_);
    current_thread_ = std::this_thread::get_id();
}

bool
Window::should_close() const {
    return glfwWindowShouldClose(window_);
}

void
Window::set_should_close(bool value) const {
    glfwSetWindowShouldClose(window_, value);
}

void
Window::set_title(const std::string &title) const {
    MAIN_THREAD_ONLY();
    glfwSetWindowTitle(window_, title.c_str());
}

void
Window::set_icon(std::vector<Image> &&images) const {
    MAIN_THREAD_ONLY();
    std::vector<GLFWimage> imgs;
    imgs.reserve(images.size());
    std::transform(
        images.begin(),
        images.end(),
        std::back_inserter(imgs),
        [](Image &image) -> GLFWimage {
            return {image.width, image.height, image.pixels.data()};
        });
    glfwSetWindowIcon(window_, static_cast<int>(imgs.size()), imgs.data());
}

std::tuple<int, int>
Window::get_pos() const {
    MAIN_THREAD_ONLY();
    int x, y;
    glfwGetWindowPos(window_, &x, &y);
    return {x, y};
}

void
Window::set_pos(int x, int y) const {
    MAIN_THREAD_ONLY();
    glfwSetWindowPos(window_, x, y);
}

std::tuple<int, int>
Window::get_size() const {
    MAIN_THREAD_ONLY();
    int x, y;
    glfwGetWindowSize(window_, &x, &y);
    return {x, y};
}

void
Window::set_size_limits(int min_width, int min_height, int max_width, int max_height) const {
    MAIN_THREAD_ONLY();
    glfwSetWindowSizeLimits(window_, min_width, min_height, max_width, max_height);
}

void
Window::set_aspect_ratio(int numerator, int denominator) const {
    MAIN_THREAD_ONLY();
    glfwSetWindowAspectRatio(window_, numerator, denominator);
}

void
Window::set_size(int x, int y) const {
    MAIN_THREAD_ONLY();
    glfwSetWindowSize(window_, x, y);
}

std::tuple<int, int>
Window::get_framebuffer_size() const {
    MAIN_THREAD_ONLY();
    int w, h;
    glfwGetFramebufferSize(window_, &w, &h);
    return {w, h};
}

std::tuple<int, int, int, int>
Window::get_frame_size() const {
    MAIN_THREAD_ONLY();
    int left, top, right, bottom;
    glfwGetWindowFrameSize(window_, &left, &top, &right, &bottom);
    return {left, top, right, bottom};
}

std::tuple<float, float>
Window::get_content_scale() const {
    MAIN_THREAD_ONLY();
    float x, y;
    glfwGetWindowContentScale(window_, &x, &y);
    return {x, y};
}

float
Window::get_opacity() const {
    MAIN_THREAD_ONLY();
    return glfwGetWindowOpacity(window_);
}

void
Window::set_opacity(float opacity) const {
    MAIN_THREAD_ONLY();
    glfwSetWindowOpacity(window_, opacity);
}

void
Window::iconify() const {
    MAIN_THREAD_ONLY();
    glfwIconifyWindow(window_);
}

void
Window::maximize() const {
    MAIN_THREAD_ONLY();
    glfwMaximizeWindow(window_);
}

void
Window::restore() const {
    MAIN_THREAD_ONLY();
    glfwRestoreWindow(window_);
}

void
Window::show() const {
    MAIN_THREAD_ONLY();
    glfwShowWindow(window_);
}

void
Window::hide() const {
    MAIN_THREAD_ONLY();
    glfwHideWindow(window_);
}

void
Window::focus() const {
    MAIN_THREAD_ONLY();
    glfwFocusWindow(window_);
}

void
Window::request_attention() const {
    MAIN_THREAD_ONLY();
    glfwRequestWindowAttention(window_);
}

Monitor
Window::get_monitor() const {
    MAIN_THREAD_ONLY();
    return glfwGetWindowMonitor(window_);
}

void
Window::set_monitor(const Monitor &monitor, int width, int height, int refresh_rate) const {
    MAIN_THREAD_ONLY();
    glfwSetWindowMonitor(window_, monitor.monitor_, 0, 0, width, height, refresh_rate);
}

void
Window::set_windowed(int x, int y, int width, int height) const {
    MAIN_THREAD_ONLY();
    glfwSetWindowMonitor(window_, nullptr, x, y, width, height, 0);
}

int
Window::get_attribute(Attribute attrib) const {
    MAIN_THREAD_ONLY();
    return glfwGetWindowAttrib(window_, static_cast<int>(attrib));
}

void
Window::set_attribute(Attribute attrib, int value) const {
    MAIN_THREAD_ONLY();
    glfwSetWindowAttrib(window_, static_cast<int>(attrib), value);
}

Window::PosFun
Window::set_position_callback(const PosFun &callback) {
    return std::exchange(pos_fun_, callback);
}

Window::SizeFun
Window::set_size_callback(const SizeFun &callback) {
    return std::exchange(size_fun_, callback);
}

Window::CloseFun
Window::set_close_callback(const CloseFun &callback) {
    return std::exchange(close_fun_, callback);
}

Window::RefreshFun
Window::set_refresh_callback(const RefreshFun &callback) {
    return std::exchange(refresh_fun_, callback);
}

Window::FocusFun
Window::set_focus_callback(const FocusFun &callback) {
    return std::exchange(focus_fun_, callback);
}

Window::IconifyFun
Window::set_iconfiy_callback(const IconifyFun &callback) {
    return std::exchange(iconify_fun_, callback);
}

Window::MaximizeFun
Window::set_maximize_callback(const MaximizeFun &callback) {
    return std::exchange(maximize_fun_, callback);
}

Window::FramebufferSizeFun
Window::set_framebuffer_size_callback(const FramebufferSizeFun &callback) {
    return std::exchange(framebuffer_size_fun_, callback);
}

Window::ContentScaleFun
Window::set_content_scale_callback(const ContentScaleFun &callback) {
    return std::exchange(content_scale_fun_, callback);
}

Window::MouseButtonFun
Window::set_mouse_button_callback(const MouseButtonFun &callback) {
    return std::exchange(mouse_button_fun_, callback);
}

Window::CursorPosFun
Window::set_cursor_pos_callback(const CursorPosFun &callback) {
    return std::exchange(cursor_pos_fun_, callback);
}

Window::CursorEnterFun
Window::set_cursor_enter_callback(const CursorEnterFun &callback) {
    return std::exchange(cursor_enter_fun_, callback);
}

Window::ScrollFun
Window::set_scroll_callback(const ScrollFun &callback) {
    return std::exchange(scroll_fun_, callback);
}

Window::KeyFun
Window::set_key_callback(const KeyFun &callback) {
    return std::exchange(key_fun_, callback);
}

Window::KeyFun
Window::set_key_callback(Key key, bool exclusive, const KeyFun &callback) {
    auto opt                        = key_map_[static_cast<int>(key)];
    key_map_[static_cast<int>(key)] = {callback, exclusive};
    KeyFun ret;
    if (opt) ret = opt->first;
    return ret;
}

Window::CharFun
Window::set_char_callback(const CharFun &callback) {
    return std::exchange(char_fun_, callback);
}

Window::DropFun
Window::set_drop_callback(const DropFun &callback) {
    return std::exchange(drop_fun_, callback);
}

void
Window::swap_buffers() const {
    glfwSwapBuffers(window_);
}

bool
Window::get_input_mode(InputMode mode) const {
    MAIN_THREAD_ONLY();
    return glfwGetInputMode(window_, static_cast<int>(mode));
}

CursorInputMode
Window::get_cursor_input_mode() const {
    MAIN_THREAD_ONLY();
    return static_cast<CursorInputMode>(glfwGetInputMode(window_, GLFW_CURSOR));
}

void
Window::set_input_mode(InputMode mode, bool value) {
    MAIN_THREAD_ONLY();
    glfwSetInputMode(window_, static_cast<int>(mode), value);
}

void
Window::set_cursor_input_mode(CursorInputMode value) {
    MAIN_THREAD_ONLY();
    glfwSetInputMode(window_, GLFW_CURSOR, static_cast<int>(value));
}

Action
Window::get_key(Key key) const {
    MAIN_THREAD_ONLY();
    return static_cast<Action>(glfwGetKey(window_, static_cast<int>(key)));
}

Action
Window::get_mouse_button(Button button) const {
    MAIN_THREAD_ONLY();
    return static_cast<Action>(glfwGetMouseButton(window_, static_cast<int>(button)));
}

std::tuple<double, double>
Window::get_cursor_pos() const {
    MAIN_THREAD_ONLY();
    double x, y;
    glfwGetCursorPos(window_, &x, &y);
    return {x, y};
}

void
Window::set_cursor_pos(double x, double y) const {
    MAIN_THREAD_ONLY();
    glfwSetCursorPos(window_, x, y);
}

void
Window::set_clipboard_string(const std::string &str) const {
    MAIN_THREAD_ONLY();
    glfwSetClipboardString(window_, str.c_str());
}

std::string
Window::get_clipboard_string() const {
    MAIN_THREAD_ONLY();
    return glfwGetClipboardString(window_);
}

void
Window::set_cursor(const Cursor &cursor) const {
    glfwSetCursor(window_, cursor());
}

void
Init::hint(int hint, int value) {
    MAIN_THREAD_ONLY();
    glfwInitHint(hint, value);
}

void
Init::hint(InitHint hint, int value) {
    MAIN_THREAD_ONLY();
    glfwInitHint(static_cast<int>(hint), value);
}

void
Init::initialize() {
    static Init init;
}

double
get_time() {
    return glfwGetTime();
}

void
set_time(double time) {
    glfwSetTime(time);
}

uint64_t
get_timer_value() {
    return glfwGetTimerValue();
}

uint64_t
get_timer_frequency() {
    return glfwGetTimerFrequency();
}

void
poll_events() {
    MAIN_THREAD_ONLY();
    glfwPollEvents();
}

void
wait_events() {
    MAIN_THREAD_ONLY();
    glfwWaitEvents();
}

void
wait_events_timeout(double timeout) {
    MAIN_THREAD_ONLY();
    glfwWaitEventsTimeout(timeout);
}

void
post_empty_event() {
    glfwPostEmptyEvent();
}

bool
raw_mouse_motion_supported() {
    MAIN_THREAD_ONLY();
    return glfwRawMouseMotionSupported();
}

std::string
get_key_name(Key key, int scancode) {
    MAIN_THREAD_ONLY();
    auto name = glfwGetKeyName(static_cast<int>(key), scancode);
    return name ? name : "";
}

int
get_key_scancode(Key key) {
    return glfwGetKeyScancode(static_cast<int>(key));
}

void
set_swap_interval(int interval) {
    glfwSwapInterval(interval);
}

bool
extension_supported(const std::string &extension) {
    return glfwExtensionSupported(extension.c_str());
}

GLProc
get_proc_address(const std::string &proc_name) {
    return glfwGetProcAddress(proc_name.c_str());
}

#define VERIFY_ENUM(ns, name) static_assert(static_cast<int>(ns::name) == GLFW_##name)

VERIFY_ENUM(InitHint, JOYSTICK_HAT_BUTTONS);
VERIFY_ENUM(InitHint, COCOA_CHDIR_RESOURCES);
VERIFY_ENUM(InitHint, COCOA_MENUBAR);

VERIFY_ENUM(WindowHint, RESIZABLE);
VERIFY_ENUM(WindowHint, VISIBLE);
VERIFY_ENUM(WindowHint, DECORATED);
VERIFY_ENUM(WindowHint, FOCUSED);
VERIFY_ENUM(WindowHint, AUTO_ICONIFY);
VERIFY_ENUM(WindowHint, FLOATING);
VERIFY_ENUM(WindowHint, MAXIMIZED);
VERIFY_ENUM(WindowHint, CENTER_CURSOR);
VERIFY_ENUM(WindowHint, TRANSPARENT_FRAMEBUFFER);
VERIFY_ENUM(WindowHint, FOCUS_ON_SHOW);
VERIFY_ENUM(WindowHint, SCALE_TO_MONITOR);
VERIFY_ENUM(WindowHint, STEREO);
VERIFY_ENUM(WindowHint, SRGB_CAPABLE);
VERIFY_ENUM(WindowHint, DOUBLEBUFFER);
VERIFY_ENUM(WindowHint, OPENGL_FORWARD_COMPAT);
VERIFY_ENUM(WindowHint, OPENGL_DEBUG_CONTEXT);
VERIFY_ENUM(WindowHint, CONTEXT_NO_ERROR);
VERIFY_ENUM(WindowHint, COCOA_RETINA_FRAMEBUFFER);
VERIFY_ENUM(WindowHint, COCOA_GRAPHICS_SWITCHING);
VERIFY_ENUM(WindowHint, RED_BITS);
VERIFY_ENUM(WindowHint, GREEN_BITS);
VERIFY_ENUM(WindowHint, BLUE_BITS);
VERIFY_ENUM(WindowHint, ALPHA_BITS);
VERIFY_ENUM(WindowHint, DEPTH_BITS);
VERIFY_ENUM(WindowHint, STENCIL_BITS);
VERIFY_ENUM(WindowHint, ACCUM_RED_BITS);
VERIFY_ENUM(WindowHint, ACCUM_GREEN_BITS);
VERIFY_ENUM(WindowHint, ACCUM_BLUE_BITS);
VERIFY_ENUM(WindowHint, ACCUM_ALPHA_BITS);
VERIFY_ENUM(WindowHint, AUX_BUFFERS);
VERIFY_ENUM(WindowHint, SAMPLES);
VERIFY_ENUM(WindowHint, REFRESH_RATE);
VERIFY_ENUM(WindowHint, CONTEXT_VERSION_MAJOR);
VERIFY_ENUM(WindowHint, CONTEXT_VERSION_MINOR);
VERIFY_ENUM(WindowHint, COCOA_FRAME_NAME);
VERIFY_ENUM(WindowHint, X11_CLASS_NAME);
VERIFY_ENUM(WindowHint, X11_INSTANCE_NAME);
VERIFY_ENUM(WindowHint, CLIENT_API);
VERIFY_ENUM(WindowHint, CONTEXT_CREATION_API);
VERIFY_ENUM(WindowHint, OPENGL_PROFILE);
VERIFY_ENUM(WindowHint, CONTEXT_ROBUSTNESS);
VERIFY_ENUM(WindowHint, CONTEXT_RELEASE_BEHAVIOR);

VERIFY_ENUM(ClientAPI, OPENGL_API);
VERIFY_ENUM(ClientAPI, OPENGL_ES_API);
VERIFY_ENUM(ClientAPI, NO_API);

VERIFY_ENUM(ContextCreationAPI, NATIVE_CONTEXT_API);
VERIFY_ENUM(ContextCreationAPI, EGL_CONTEXT_API);
VERIFY_ENUM(ContextCreationAPI, OSMESA_CONTEXT_API);

VERIFY_ENUM(OpenGLProfile, OPENGL_CORE_PROFILE);
VERIFY_ENUM(OpenGLProfile, OPENGL_COMPAT_PROFILE);
VERIFY_ENUM(OpenGLProfile, OPENGL_ANY_PROFILE);

VERIFY_ENUM(ContextRobustness, NO_RESET_NOTIFICATION);
VERIFY_ENUM(ContextRobustness, LOSE_CONTEXT_ON_RESET);
VERIFY_ENUM(ContextRobustness, NO_ROBUSTNESS);

VERIFY_ENUM(ContextReleaseBehavior, ANY_RELEASE_BEHAVIOR);
VERIFY_ENUM(ContextReleaseBehavior, RELEASE_BEHAVIOR_FLUSH);
VERIFY_ENUM(ContextReleaseBehavior, RELEASE_BEHAVIOR_NONE);

VERIFY_ENUM(Attribute, FOCUSED);
VERIFY_ENUM(Attribute, ICONIFIED);
VERIFY_ENUM(Attribute, MAXIMIZED);
VERIFY_ENUM(Attribute, HOVERED);
VERIFY_ENUM(Attribute, VISIBLE);
VERIFY_ENUM(Attribute, RESIZABLE);
VERIFY_ENUM(Attribute, DECORATED);
VERIFY_ENUM(Attribute, AUTO_ICONIFY);
VERIFY_ENUM(Attribute, FLOATING);
VERIFY_ENUM(Attribute, TRANSPARENT_FRAMEBUFFER);
VERIFY_ENUM(Attribute, FOCUS_ON_SHOW);
VERIFY_ENUM(Attribute, CLIENT_API);
VERIFY_ENUM(Attribute, CONTEXT_CREATION_API);
VERIFY_ENUM(Attribute, CONTEXT_VERSION_MAJOR);
VERIFY_ENUM(Attribute, OPENGL_FORWARD_COMPAT);
VERIFY_ENUM(Attribute, OPENGL_DEBUG_CONTEXT);
VERIFY_ENUM(Attribute, OPENGL_PROFILE);
VERIFY_ENUM(Attribute, CONTEXT_RELEASE_BEHAVIOR);
VERIFY_ENUM(Attribute, CONTEXT_NO_ERROR);
VERIFY_ENUM(Attribute, CONTEXT_ROBUSTNESS);

VERIFY_ENUM(InputMode, STICKY_KEYS);
VERIFY_ENUM(InputMode, STICKY_MOUSE_BUTTONS);
VERIFY_ENUM(InputMode, LOCK_KEY_MODS);
VERIFY_ENUM(InputMode, RAW_MOUSE_MOTION);

VERIFY_ENUM(CursorInputMode, CURSOR_NORMAL);
VERIFY_ENUM(CursorInputMode, CURSOR_HIDDEN);
VERIFY_ENUM(CursorInputMode, CURSOR_DISABLED);

VERIFY_ENUM(Key, KEY_UNKNOWN);
VERIFY_ENUM(Key, KEY_SPACE);
VERIFY_ENUM(Key, KEY_APOSTROPHE);
VERIFY_ENUM(Key, KEY_COMMA);
VERIFY_ENUM(Key, KEY_MINUS);
VERIFY_ENUM(Key, KEY_PERIOD);
VERIFY_ENUM(Key, KEY_SLASH);
VERIFY_ENUM(Key, KEY_0);
VERIFY_ENUM(Key, KEY_1);
VERIFY_ENUM(Key, KEY_2);
VERIFY_ENUM(Key, KEY_3);
VERIFY_ENUM(Key, KEY_4);
VERIFY_ENUM(Key, KEY_5);
VERIFY_ENUM(Key, KEY_6);
VERIFY_ENUM(Key, KEY_7);
VERIFY_ENUM(Key, KEY_8);
VERIFY_ENUM(Key, KEY_9);
VERIFY_ENUM(Key, KEY_SEMICOLON);
VERIFY_ENUM(Key, KEY_EQUAL);
VERIFY_ENUM(Key, KEY_A);
VERIFY_ENUM(Key, KEY_B);
VERIFY_ENUM(Key, KEY_C);
VERIFY_ENUM(Key, KEY_D);
VERIFY_ENUM(Key, KEY_E);
VERIFY_ENUM(Key, KEY_F);
VERIFY_ENUM(Key, KEY_G);
VERIFY_ENUM(Key, KEY_H);
VERIFY_ENUM(Key, KEY_I);
VERIFY_ENUM(Key, KEY_J);
VERIFY_ENUM(Key, KEY_K);
VERIFY_ENUM(Key, KEY_L);
VERIFY_ENUM(Key, KEY_M);
VERIFY_ENUM(Key, KEY_N);
VERIFY_ENUM(Key, KEY_O);
VERIFY_ENUM(Key, KEY_P);
VERIFY_ENUM(Key, KEY_Q);
VERIFY_ENUM(Key, KEY_R);
VERIFY_ENUM(Key, KEY_S);
VERIFY_ENUM(Key, KEY_T);
VERIFY_ENUM(Key, KEY_U);
VERIFY_ENUM(Key, KEY_V);
VERIFY_ENUM(Key, KEY_W);
VERIFY_ENUM(Key, KEY_X);
VERIFY_ENUM(Key, KEY_Y);
VERIFY_ENUM(Key, KEY_Z);
VERIFY_ENUM(Key, KEY_LEFT_BRACKET);
VERIFY_ENUM(Key, KEY_BACKSLASH);
VERIFY_ENUM(Key, KEY_RIGHT_BRACKET);
VERIFY_ENUM(Key, KEY_GRAVE_ACCENT);
VERIFY_ENUM(Key, KEY_WORLD_1);
VERIFY_ENUM(Key, KEY_WORLD_2);
VERIFY_ENUM(Key, KEY_ESCAPE);
VERIFY_ENUM(Key, KEY_ENTER);
VERIFY_ENUM(Key, KEY_TAB);
VERIFY_ENUM(Key, KEY_BACKSPACE);
VERIFY_ENUM(Key, KEY_INSERT);
VERIFY_ENUM(Key, KEY_DELETE);
VERIFY_ENUM(Key, KEY_RIGHT);
VERIFY_ENUM(Key, KEY_LEFT);
VERIFY_ENUM(Key, KEY_DOWN);
VERIFY_ENUM(Key, KEY_UP);
VERIFY_ENUM(Key, KEY_PAGE_UP);
VERIFY_ENUM(Key, KEY_PAGE_DOWN);
VERIFY_ENUM(Key, KEY_HOME);
VERIFY_ENUM(Key, KEY_END);
VERIFY_ENUM(Key, KEY_CAPS_LOCK);
VERIFY_ENUM(Key, KEY_SCROLL_LOCK);
VERIFY_ENUM(Key, KEY_NUM_LOCK);
VERIFY_ENUM(Key, KEY_PRINT_SCREEN);
VERIFY_ENUM(Key, KEY_PAUSE);
VERIFY_ENUM(Key, KEY_F1);
VERIFY_ENUM(Key, KEY_F2);
VERIFY_ENUM(Key, KEY_F3);
VERIFY_ENUM(Key, KEY_F4);
VERIFY_ENUM(Key, KEY_F5);
VERIFY_ENUM(Key, KEY_F6);
VERIFY_ENUM(Key, KEY_F7);
VERIFY_ENUM(Key, KEY_F8);
VERIFY_ENUM(Key, KEY_F9);
VERIFY_ENUM(Key, KEY_F10);
VERIFY_ENUM(Key, KEY_F11);
VERIFY_ENUM(Key, KEY_F12);
VERIFY_ENUM(Key, KEY_F13);
VERIFY_ENUM(Key, KEY_F14);
VERIFY_ENUM(Key, KEY_F15);
VERIFY_ENUM(Key, KEY_F16);
VERIFY_ENUM(Key, KEY_F17);
VERIFY_ENUM(Key, KEY_F18);
VERIFY_ENUM(Key, KEY_F19);
VERIFY_ENUM(Key, KEY_F20);
VERIFY_ENUM(Key, KEY_F21);
VERIFY_ENUM(Key, KEY_F22);
VERIFY_ENUM(Key, KEY_F23);
VERIFY_ENUM(Key, KEY_F24);
VERIFY_ENUM(Key, KEY_F25);
VERIFY_ENUM(Key, KEY_KP_0);
VERIFY_ENUM(Key, KEY_KP_1);
VERIFY_ENUM(Key, KEY_KP_2);
VERIFY_ENUM(Key, KEY_KP_3);
VERIFY_ENUM(Key, KEY_KP_4);
VERIFY_ENUM(Key, KEY_KP_5);
VERIFY_ENUM(Key, KEY_KP_6);
VERIFY_ENUM(Key, KEY_KP_7);
VERIFY_ENUM(Key, KEY_KP_8);
VERIFY_ENUM(Key, KEY_KP_9);
VERIFY_ENUM(Key, KEY_KP_DECIMAL);
VERIFY_ENUM(Key, KEY_KP_DIVIDE);
VERIFY_ENUM(Key, KEY_KP_MULTIPLY);
VERIFY_ENUM(Key, KEY_KP_SUBTRACT);
VERIFY_ENUM(Key, KEY_KP_ADD);
VERIFY_ENUM(Key, KEY_KP_ENTER);
VERIFY_ENUM(Key, KEY_KP_EQUAL);
VERIFY_ENUM(Key, KEY_LEFT_SHIFT);
VERIFY_ENUM(Key, KEY_LEFT_CONTROL);
VERIFY_ENUM(Key, KEY_LEFT_ALT);
VERIFY_ENUM(Key, KEY_LEFT_SUPER);
VERIFY_ENUM(Key, KEY_RIGHT_SHIFT);
VERIFY_ENUM(Key, KEY_RIGHT_CONTROL);
VERIFY_ENUM(Key, KEY_RIGHT_ALT);
VERIFY_ENUM(Key, KEY_RIGHT_SUPER);
VERIFY_ENUM(Key, KEY_MENU);
VERIFY_ENUM(Key, KEY_LAST);

VERIFY_ENUM(Button, MOUSE_BUTTON_1);
VERIFY_ENUM(Button, MOUSE_BUTTON_2);
VERIFY_ENUM(Button, MOUSE_BUTTON_3);
VERIFY_ENUM(Button, MOUSE_BUTTON_4);
VERIFY_ENUM(Button, MOUSE_BUTTON_5);
VERIFY_ENUM(Button, MOUSE_BUTTON_6);
VERIFY_ENUM(Button, MOUSE_BUTTON_7);
VERIFY_ENUM(Button, MOUSE_BUTTON_8);
VERIFY_ENUM(Button, MOUSE_BUTTON_LAST);
VERIFY_ENUM(Button, MOUSE_BUTTON_LEFT);
VERIFY_ENUM(Button, MOUSE_BUTTON_RIGHT);
VERIFY_ENUM(Button, MOUSE_BUTTON_MIDDLE);

VERIFY_ENUM(Action, RELEASE);
VERIFY_ENUM(Action, PRESS);
VERIFY_ENUM(Action, REPEAT);

VERIFY_ENUM(Modifier, MOD_SHIFT);
VERIFY_ENUM(Modifier, MOD_CONTROL);
VERIFY_ENUM(Modifier, MOD_ALT);
VERIFY_ENUM(Modifier, MOD_SUPER);
VERIFY_ENUM(Modifier, MOD_CAPS_LOCK);
VERIFY_ENUM(Modifier, MOD_NUM_LOCK);

} // namespace GLFW
