
// Created by taylor-santos on 6/14/2022 at 09:50.

#pragma once

#include <thread>
#include <functional>

struct GLFWmonitor;
struct GLFWwindow;

namespace GLFW {

enum class InitHint {
    JOYSTICK_HAT_BUTTONS  = 0x00050001,
    COCOA_CHDIR_RESOURCES = 0x00051001,
    COCOA_MENUBAR         = 0x00051002,
};

enum class WindowHint {
    ////////////////
    // Bool Hints //
    ////////////////
    // Window related hints
    RESIZABLE               = 0x00020003,
    VISIBLE                 = 0x00020004,
    DECORATED               = 0x00020005,
    FOCUSED                 = 0x00020001,
    AUTO_ICONIFY            = 0x00020006,
    FLOATING                = 0x00020007,
    MAXIMIZED               = 0x00020008,
    CENTER_CURSOR           = 0x00020009,
    TRANSPARENT_FRAMEBUFFER = 0x0002000A,
    FOCUS_ON_SHOW           = 0x0002000C,
    SCALE_TO_MONITOR        = 0x0002200C,

    // Framebuffer related hints
    STEREO       = 0x0002100C,
    SRGB_CAPABLE = 0x0002100E,
    DOUBLEBUFFER = 0x00021010,

    // Context related hints
    OPENGL_FORWARD_COMPAT = 0x00022006,
    OPENGL_DEBUG_CONTEXT  = 0x00022007,
    CONTEXT_NO_ERROR      = 0x0002200A,

    // MacOS specific window hints
    COCOA_RETINA_FRAMEBUFFER = 0x00023001,
    COCOA_GRAPHICS_SWITCHING = 0x00023003,

    ///////////////
    // Int Hints //
    ///////////////
    // Framebuffer related hints
    RED_BITS         = 0x00021001,
    GREEN_BITS       = 0x00021002,
    BLUE_BITS        = 0x00021003,
    ALPHA_BITS       = 0x00021004,
    DEPTH_BITS       = 0x00021005,
    STENCIL_BITS     = 0x00021006,
    ACCUM_RED_BITS   = 0x00021007,
    ACCUM_GREEN_BITS = 0x00021008,
    ACCUM_BLUE_BITS  = 0x00021009,
    ACCUM_ALPHA_BITS = 0x0002100A,
    AUX_BUFFERS      = 0x0002100B,
    SAMPLES          = 0x0002100D,

    // Monitor related hints
    REFRESH_RATE = 0x0002100F,

    // Context related hints
    CONTEXT_VERSION_MAJOR = 0x00022002,
    CONTEXT_VERSION_MINOR = 0x00022003,

    //////////////////
    // String Hints //
    //////////////////
    // MacOS specific window hints
    COCOA_FRAME_NAME = 0x00023002,

    // X11 specific window hints
    X11_CLASS_NAME    = 0x00024001,
    X11_INSTANCE_NAME = 0x00024002,

    ////////////////
    // Enum Hints //
    ////////////////
    // Context related hints
    CLIENT_API               = 0,
    CONTEXT_CREATION_API     = 0x0002200B,
    OPENGL_PROFILE           = 0x00022008,
    CONTEXT_ROBUSTNESS       = 0x00022005,
    CONTEXT_RELEASE_BEHAVIOR = 0x00022009,
};

enum class ClientAPI {
    OPENGL_API    = 0x00030001,
    OPENGL_ES_API = 0x00030002,
    NO_API        = 0,
};

enum class ContextCreationAPI {
    NATIVE_CONTEXT_API = 0,
    EGL_CONTEXT_API    = 0,
    OSMESA_CONTEXT_API = 0,
};

enum class OpenGLProfile {
    OPENGL_CORE_PROFILE   = 0,
    OPENGL_COMPAT_PROFILE = 0,
    OPENGL_ANY_PROFILE    = 0,
};

enum class ContextRobustness {
    NO_RESET_NOTIFICATION = 0,
    LOSE_CONTEXT_ON_RESET = 0,
    NO_ROBUSTNESS         = 0,
};

enum class ContextReleaseBehavior {
    ANY_RELEASE_BEHAVIOR   = 0,
    RELEASE_BEHAVIOR_FLUSH = 0,
    RELEASE_BEHAVIOR_NONE  = 0,
};

enum class Attribute {
    // Window related attributes
    FOCUSED                 = 0x00020001,
    ICONIFIED               = 0x00020002,
    MAXIMIZED               = 0x00020008,
    HOVERED                 = 0x0002000B,
    VISIBLE                 = 0x00020004,
    RESIZABLE               = 0x00020003,
    DECORATED               = 0x00020005,
    AUTO_ICONIFY            = 0x00020006,
    FLOATING                = 0x00020007,
    TRANSPARENT_FRAMEBUFFER = 0x0002000A,
    FOCUS_ON_SHOW           = 0x0002000C,

    // Context related attributes
    CLIENT_API               = 0x00022001,
    CONTEXT_CREATION_API     = 0x0002200B,
    CONTEXT_VERSION_MAJOR    = 0x00022002,
    OPENGL_FORWARD_COMPAT    = 0x00022006,
    OPENGL_DEBUG_CONTEXT     = 0x00022007,
    OPENGL_PROFILE           = 0x00022008,
    CONTEXT_RELEASE_BEHAVIOR = 0x00022009,
    CONTEXT_NO_ERROR         = 0x0002200A,
    CONTEXT_ROBUSTNESS       = 0x00022005,
};

enum class InputMode {
    STICKY_KEYS          = 0x00033002,
    STICKY_MOUSE_BUTTONS = 0x00033003,
    LOCK_KEY_MODS        = 0x00033004,
    RAW_MOUSE_MOTION     = 0x00033005,
};

enum class CursorInputMode {
    CURSOR_NORMAL   = 0x00034001,
    CURSOR_HIDDEN   = 0x00034002,
    CURSOR_DISABLED = 0x00034003,
};

enum class Key {
    KEY_UNKNOWN       = -1,
    KEY_SPACE         = 32,
    KEY_APOSTROPHE    = 39,
    KEY_COMMA         = 44,
    KEY_MINUS         = 45,
    KEY_PERIOD        = 46,
    KEY_SLASH         = 47,
    KEY_0             = 48,
    KEY_1             = 49,
    KEY_2             = 50,
    KEY_3             = 51,
    KEY_4             = 52,
    KEY_5             = 53,
    KEY_6             = 54,
    KEY_7             = 55,
    KEY_8             = 56,
    KEY_9             = 57,
    KEY_SEMICOLON     = 59,
    KEY_EQUAL         = 61,
    KEY_A             = 65,
    KEY_B             = 66,
    KEY_C             = 67,
    KEY_D             = 68,
    KEY_E             = 69,
    KEY_F             = 70,
    KEY_G             = 71,
    KEY_H             = 72,
    KEY_I             = 73,
    KEY_J             = 74,
    KEY_K             = 75,
    KEY_L             = 76,
    KEY_M             = 77,
    KEY_N             = 78,
    KEY_O             = 79,
    KEY_P             = 80,
    KEY_Q             = 81,
    KEY_R             = 82,
    KEY_S             = 83,
    KEY_T             = 84,
    KEY_U             = 85,
    KEY_V             = 86,
    KEY_W             = 87,
    KEY_X             = 88,
    KEY_Y             = 89,
    KEY_Z             = 90,
    KEY_LEFT_BRACKET  = 91,
    KEY_BACKSLASH     = 92,
    KEY_RIGHT_BRACKET = 93,
    KEY_GRAVE_ACCENT  = 96,
    KEY_WORLD_1       = 161,
    KEY_WORLD_2       = 162,
    KEY_ESCAPE        = 256,
    KEY_ENTER         = 257,
    KEY_TAB           = 258,
    KEY_BACKSPACE     = 259,
    KEY_INSERT        = 260,
    KEY_DELETE        = 261,
    KEY_RIGHT         = 262,
    KEY_LEFT          = 263,
    KEY_DOWN          = 264,
    KEY_UP            = 265,
    KEY_PAGE_UP       = 266,
    KEY_PAGE_DOWN     = 267,
    KEY_HOME          = 268,
    KEY_END           = 269,
    KEY_CAPS_LOCK     = 280,
    KEY_SCROLL_LOCK   = 281,
    KEY_NUM_LOCK      = 282,
    KEY_PRINT_SCREEN  = 283,
    KEY_PAUSE         = 284,
    KEY_F1            = 290,
    KEY_F2            = 291,
    KEY_F3            = 292,
    KEY_F4            = 293,
    KEY_F5            = 294,
    KEY_F6            = 295,
    KEY_F7            = 296,
    KEY_F8            = 297,
    KEY_F9            = 298,
    KEY_F10           = 299,
    KEY_F11           = 300,
    KEY_F12           = 301,
    KEY_F13           = 302,
    KEY_F14           = 303,
    KEY_F15           = 304,
    KEY_F16           = 305,
    KEY_F17           = 306,
    KEY_F18           = 307,
    KEY_F19           = 308,
    KEY_F20           = 309,
    KEY_F21           = 310,
    KEY_F22           = 311,
    KEY_F23           = 312,
    KEY_F24           = 313,
    KEY_F25           = 314,
    KEY_KP_0          = 320,
    KEY_KP_1          = 321,
    KEY_KP_2          = 322,
    KEY_KP_3          = 323,
    KEY_KP_4          = 324,
    KEY_KP_5          = 325,
    KEY_KP_6          = 326,
    KEY_KP_7          = 327,
    KEY_KP_8          = 328,
    KEY_KP_9          = 329,
    KEY_KP_DECIMAL    = 330,
    KEY_KP_DIVIDE     = 331,
    KEY_KP_MULTIPLY   = 332,
    KEY_KP_SUBTRACT   = 333,
    KEY_KP_ADD        = 334,
    KEY_KP_ENTER      = 335,
    KEY_KP_EQUAL      = 336,
    KEY_LEFT_SHIFT    = 340,
    KEY_LEFT_CONTROL  = 341,
    KEY_LEFT_ALT      = 342,
    KEY_LEFT_SUPER    = 343,
    KEY_RIGHT_SHIFT   = 344,
    KEY_RIGHT_CONTROL = 345,
    KEY_RIGHT_ALT     = 346,
    KEY_RIGHT_SUPER   = 347,
    KEY_MENU          = 348,
    KEY_LAST          = KEY_MENU,
};

enum class Button {
    MOUSE_BUTTON_1      = 0,
    MOUSE_BUTTON_2      = 1,
    MOUSE_BUTTON_3      = 2,
    MOUSE_BUTTON_4      = 3,
    MOUSE_BUTTON_5      = 4,
    MOUSE_BUTTON_6      = 5,
    MOUSE_BUTTON_7      = 6,
    MOUSE_BUTTON_8      = 7,
    MOUSE_BUTTON_LAST   = MOUSE_BUTTON_8,
    MOUSE_BUTTON_LEFT   = MOUSE_BUTTON_1,
    MOUSE_BUTTON_RIGHT  = MOUSE_BUTTON_2,
    MOUSE_BUTTON_MIDDLE = MOUSE_BUTTON_3,
};

enum class Action {
    RELEASE = 0,
    PRESS   = 1,
    REPEAT  = 2,
};

// Non-class enum to allow for easy bitwise operations with & and |
enum Modifier {
    MOD_SHIFT     = 0x0001,
    MOD_CONTROL   = 0x0002,
    MOD_ALT       = 0x0004,
    MOD_SUPER     = 0x0008,
    MOD_CAPS_LOCK = 0x0010,
    MOD_NUM_LOCK  = 0x0020,
};

class Error : public std::runtime_error {
public:
    Error(int code, const char *msg);

    ~Error() override = default;

    [[nodiscard]] const char *
    what() const noexcept override;

private:
    std::string message_;
};

class Init {
public:
    static void
    hint(InitHint hint, int value);

    static void
    initialize();

private:
    Init();
    ~Init();
};

struct Vidmode {
    int width;
    int height;
    int red_bits;
    int green_bits;
    int blue_bits;
    int refresh_rate;
};

struct GammaRamp {
    unsigned short red;
    unsigned short green;
    unsigned short blue;
};

class Monitor {
public:
    using Fun = std::function<void(int)>;

    // This function returns a vector of all currently connected monitors. The
    // primary monitor is always first in the returned array. This function must
    // only be called from the main thread.
    static std::vector<Monitor>
    get_all();

    // This function returns the primary monitor.
    // This function must only be called from the main thread.
    static Monitor
    get_primary();

    // This function returns the position, in screen coordinates, of the upper-left corner of the
    // specified monitor.
    // This function must only be called from the main thread.
    [[nodiscard]] std::pair<int, int>
    get_pos() const;

    // This function returns the position, in screen coordinates, of the upper-left corner of the
    // work area of the specified monitor along with the work area size in screen coordinates. The
    // work area is defined as the area of the monitor not occluded by the operating system task bar
    // where present. If no task bar exists then the work area is the monitor resolution in screen
    // coordinates.
    // This function must only be called from the main thread.
    [[nodiscard]] std::tuple<int, int, int, int>
    get_workarea() const;

    // This function returns the size, in millimetres, of the display area of the specified monitor.
    // This function must only be called from the main thread.
    [[nodiscard]] std::pair<int, int>
    get_physical_size() const;

    // This function retrieves the content scale for the specified monitor. The content scale is the
    // ratio between the current DPI and the platform's default DPI.
    // This function must only be called from the main thread.
    [[nodiscard]] std::pair<float, float>
    get_content_scale() const;

    // This function returns a human-readable name, encoded as UTF-8, of the specified monitor.
    // This function must only be called from the main thread.
    [[nodiscard]] std::string
    get_name() const;

    // This function sets the monitor configuration callback. This is called when a monitor is
    // connected to or disconnected from the system.
    // This function must only be called from the main thread.
    Fun
    set_callback(const Fun &callback);

    // This function returns an array of all video modes supported by the specified monitor. The
    // returned array is sorted in ascending order, first by color bit depth (the sum of all channel
    // depths), then by resolution area (the product of width and height), then resolution width and
    // finally by refresh rate.
    // This function must only be called from the main thread.
    [[nodiscard]] std::vector<Vidmode>
    get_video_modes() const;

    // This function returns the current video mode of the specified monitor. If you have created a
    // full screen window for that monitor, the return value will depend on whether that window is
    // iconified.
    // This function must only be called from the main thread.
    [[nodiscard]] Vidmode
    get_video_mode() const;

    // This function generates an appropriately sized gamma ramp from the specified exponent and
    // then calls Monitor::set_gamma_ramp with it. The value must be a finite number greater than
    // zero.
    // This function must only be called from the main thread.
    void
    set_gamma(float gamma);

    // This function returns the current gamma ramp of the specified monitor.
    // This function must only be called from the main thread.
    [[nodiscard]] std::vector<GammaRamp>
    get_gamma_ramp() const;

    // This function sets the current gamma ramp for the specified monitor. The original gamma ramp
    // for that monitor is saved by GLFW the first time this function is called.
    // This function must only be called from the main thread.
    void
    set_gamma_ramp(const std::vector<GammaRamp> &gamma_ramp);

    GLFWmonitor *
    operator()() const;

private:
    Monitor(GLFWmonitor *monitor);

    GLFWmonitor *monitor_;
    Fun          callback_{};

    friend class Window;
};

class Window {
public:
    using PosFun             = std::function<void(int x, int y)>;
    using SizeFun            = std::function<void(int w, int h)>;
    using CloseFun           = std::function<void()>;
    using RefreshFun         = std::function<void()>;
    using FocusFun           = std::function<void(bool focused)>;
    using IconifyFun         = std::function<void(bool iconified)>;
    using MaximizeFun        = std::function<void(bool maximized)>;
    using FramebufferSizeFun = std::function<void(int w, int h)>;
    using ContentScaleFun    = std::function<void(float x, float y)>;

    using MouseButtonFun = std::function<void(Button button, Action action, Modifier mods)>;
    using CursorPosFun   = std::function<void(double x, double y)>;
    using CursorEnterFun = std::function<void(bool entered)>;
    using ScrollFun      = std::function<void(double x, double y)>;
    using KeyFun         = std::function<void(Key key, int scancode, Action action, Modifier mods)>;
    using CharFun        = std::function<void(unsigned int codepoint)>;
    using DropFun        = std::function<void(const std::vector<std::string> &paths)>;

    // This function resets all window hints to their default values.
    // This function must only be called from the main thread.
    static void
    default_hints();

    // This function sets hints for the next call to the Window constructor. The hints, once set,
    // retain their values until changed by a call to this function or Window::default_hints, or
    // until the library is terminated.
    // This function must only be called from the main thread.
    template<WindowHint H, typename T>
    static void
    hint(T value) = delete;

    template<WindowHint H>
    requires(
        H == WindowHint::RESIZABLE || H == WindowHint::VISIBLE || H == WindowHint::DECORATED ||
        H == WindowHint::FOCUSED || H == WindowHint::AUTO_ICONIFY || H == WindowHint::FLOATING ||
        H == WindowHint::MAXIMIZED || H == WindowHint::CENTER_CURSOR ||
        H == WindowHint::TRANSPARENT_FRAMEBUFFER || H == WindowHint::FOCUS_ON_SHOW ||
        H == WindowHint::SCALE_TO_MONITOR || H == WindowHint::STEREO ||
        H == WindowHint::SRGB_CAPABLE || H == WindowHint::DOUBLEBUFFER ||
        H == WindowHint::OPENGL_FORWARD_COMPAT || H == WindowHint::OPENGL_DEBUG_CONTEXT ||
        H == WindowHint::CONTEXT_NO_ERROR || H == WindowHint::COCOA_RETINA_FRAMEBUFFER ||
        H == WindowHint::COCOA_GRAPHICS_SWITCHING) static void hint(bool value) {
        hint_(static_cast<int>(H), value);
    }

    template<WindowHint H>
    requires(
        H == WindowHint::RED_BITS || H == WindowHint::GREEN_BITS || H == WindowHint::BLUE_BITS ||
        H == WindowHint::ALPHA_BITS || H == WindowHint::DEPTH_BITS ||
        H == WindowHint::STENCIL_BITS || H == WindowHint::ACCUM_RED_BITS ||
        H == WindowHint::ACCUM_GREEN_BITS || H == WindowHint::ACCUM_BLUE_BITS ||
        H == WindowHint::ACCUM_ALPHA_BITS || H == WindowHint::AUX_BUFFERS ||
        H == WindowHint::SAMPLES || H == WindowHint::REFRESH_RATE ||
        H == WindowHint::CONTEXT_VERSION_MAJOR ||
        H == WindowHint::CONTEXT_VERSION_MINOR) static void hint(int value) {
        hint_(static_cast<int>(H), value);
    }

    template<WindowHint H>
    requires(
        H == WindowHint::COCOA_FRAME_NAME || H == WindowHint::X11_CLASS_NAME ||
        H == WindowHint::X11_INSTANCE_NAME) static void hint(const std::string &value) {
        hint_(static_cast<int>(H), value);
    }
    template<WindowHint H>
    requires(
        H == WindowHint::COCOA_FRAME_NAME || H == WindowHint::X11_CLASS_NAME ||
        H == WindowHint::X11_INSTANCE_NAME) static void hint(const char *value) {
        hint_(static_cast<int>(H), value);
    }

    template<WindowHint H>
    requires(H == WindowHint::CLIENT_API) static void hint(ClientAPI value) {
        hint_(static_cast<int>(H), static_cast<int>(value));
    }

    template<WindowHint H>
    requires(H == WindowHint::CONTEXT_CREATION_API) static void hint(ContextCreationAPI value) {
        hint_(static_cast<int>(H), static_cast<int>(value));
    }

    template<WindowHint H>
    requires(H == WindowHint::OPENGL_PROFILE) static void hint(OpenGLProfile value) {
        hint_(static_cast<int>(H), static_cast<int>(value));
    }

    template<WindowHint H>
    requires(H == WindowHint::CONTEXT_ROBUSTNESS) static void hint(ContextRobustness value) {
        hint_(static_cast<int>(H), static_cast<int>(value));
    }

    template<WindowHint H>
    requires(H == WindowHint::CONTEXT_RELEASE_BEHAVIOR) static void hint(
        ContextReleaseBehavior value) {
        hint_(static_cast<int>(H), static_cast<int>(value));
    }

    static const Window *
    get_current_context();

    static void
    clear_current_context();

    // This function creates a window and its associated OpenGL or OpenGL ES context. Most of the
    // options controlling how the window and its context should be created are specified with
    // window hints.
    // Successful creation does not change which context is current. Before you can use the newly
    // created context, you need to call Window::make_current.
    // To create a full screen window, you need to specify the monitor the window will cover. If no
    // monitor is specified, the window will be windowed mode.
    // This function must only be called from the main thread.
    Window(
        int         width,
        int         height,
        const char *title,
        Monitor    *monitor = nullptr,
        Window     *share   = nullptr);

    Window(const Window &) = delete;
    Window &
    operator=(const Window &) = delete;

    Window(Window &&other) noexcept;
    Window &
    operator=(Window &&other) noexcept;

    ~Window();

    GLFWwindow *
    operator()() const;

    void
    make_context_current() const;

    // This function returns the value of the close flag of the specified window.
    [[nodiscard]] bool
    should_close() const;

    // This function sets the value of the close flag of the specified window.
    void
    set_should_close(bool value) const;

    // This function sets the window title, encoded as UTF-8, of the specified window.
    // This function must only be called from the main thread.
    void
    set_title(const std::string &title) const;

    // This function sets the icon of the specified window. If passed multiple candidate images,
    // those of or closest to the sizes desired by the system are selected. If no images are
    // specified, the window reverts to its default icon.
    // This function must only be called from the main thread.
    void
    set_icon(const std::vector<void *> &images) const; // TODO unimplemented

    // This function retrieves the position, in screen coordinates, of the upper-left corner of the
    // content area of the specified window.
    // This function must only be called from the main thread.
    [[nodiscard]] std::pair<int, int>
    get_pos() const;

    // This function sets the position, in screen coordinates, of the upper-left corner of the
    // content area of the specified windowed mode window. If the window is a full screen window,
    // this function does nothing.
    // This function must only be called from the main thread.
    void
    set_pos(int x, int y) const;

    // This function retrieves the size, in screen coordinates, of the content area of the specified
    // window.
    // This function must only be called from the main thread.
    [[nodiscard]] std::pair<int, int>
    get_size() const;

    // This function sets the size limits of the content area of the specified window. If the window
    // is full screen, the size limits only take effect once it is made windowed. If the window is
    // not resizable, this function does nothing.
    // This function must only be called from the main thread.
    void
    set_size_limits(int min_width, int min_height, int max_width, int max_height) const;

    // This function sets the required aspect ratio of the content area of the specified window. If
    // the window is full screen, the aspect ratio only takes effect once it is made windowed. If
    // the window is not resizable, this function does nothing.
    // If the numerator and denominator is set to -1 then the aspect ratio limit is disabled.
    // This function must only be called from the main thread.
    void
    set_aspect_ratio(int numerator, int denominator) const;

    // This function sets the size, in screen coordinates, of the content area of the specified
    // window.
    // This function must only be called from the main thread.
    void
    set_size(int x, int y) const;

    // This function retrieves the size, in pixels, of the framebuffer of the specified window.
    // This function must only be called from the main thread.
    [[nodiscard]] std::pair<int, int>
    get_framebuffer_size() const;

    // This function retrieves the size, in screen coordinates, of each edge of the frame of the
    // specified window. This size includes the title bar, if the window has one.
    // This function must only be called from the main thread.
    [[nodiscard]] std::tuple<int, int, int, int>
    get_frame_size() const;

    // This function retrieves the content scale for the specified window. The content scale is the
    // ratio between the current DPI and the platform's default DPI.
    // This function must only be called from the main thread.
    [[nodiscard]] std::pair<float, float>
    get_content_scale() const;

    // This function returns the opacity of the window, including any decorations.
    // This function must only be called from the main thread.
    [[nodiscard]] float
    get_opacity() const;

    // This function sets the opacity of the window, including any decorations.
    // This function must only be called from the main thread.
    void
    set_opacity(float opacity) const;

    // This function iconifies (minimizes) the specified window if it was previously restored. If
    // the window is already iconified, this function does nothing.
    // This function must only be called from the main thread.
    void
    iconify() const;

    // This function restores the specified window if it was previously iconified (minimized) or
    // maximized. If the window is already restored, this function does nothing.
    // This function must only be called from the main thread.
    void
    restore() const;

    // This function maximizes the specified window if it was previously not maximized. If the
    // window is already maximized, this function does nothing.
    // This function must only be called from the main thread.
    void
    maximize() const;

    // This function makes the specified window visible if it was previously hidden. If the window
    // is already visible or is in full screen mode, this function does nothing.
    // This function must only be called from the main thread.
    void
    show() const;

    // This function hides the specified window if it was previously visible. If the window is
    // already hidden or is in full screen mode, this function does nothing.
    // This function must only be called from the main thread.
    void
    hide() const;

    // This function brings the specified window to front and sets input focus. The window should
    // already be visible and not iconified.
    // This function must only be called from the main thread.
    void
    focus() const;

    // This function requests user attention to the specified window. On platforms where this is not
    // supported, attention is requested to the application as a whole.
    // This function must only be called from the main thread.
    void
    request_attention() const;

    // This function returns the handle of the monitor that the specified window is in full screen
    // on.
    // This function must only be called from the main thread.
    [[nodiscard]] Monitor
    get_monitor() const;

    // This function sets the monitor that the window uses for full screen mode. When setting a
    // monitor, this function updates the width, height and refresh rate of the desired video mode
    // and switches to the video mode closest to it.
    // This function must only be called from the main thread.
    void
    set_monitor(const Monitor &monitor, int width, int height, int refresh_rate) const;

    // This function sets the window to windowed mode.
    // This function must only be called from the main thread.
    void
    set_windowed(int x, int y, int width, int height) const;

    // This function returns the value of an attribute of the specified window or its OpenGL or
    // OpenGL ES context.
    // This function must only be called from the main thread.
    [[nodiscard]] int
    get_attribute(Attribute attrib) const;

    // This function sets the value of an attribute of the specified window.
    // This function must only be called from the main thread.
    void
    set_attribute(Attribute attrib, int value) const;

    PosFun
    set_position_callback(PosFun callback);

    SizeFun
    set_size_callback(SizeFun callback);

    CloseFun
    set_close_callback(CloseFun callback);

    RefreshFun
    set_refresh_callback(RefreshFun callback);

    FocusFun
    set_focus_callback(FocusFun callback);

    IconifyFun
    set_iconfiy_callback(IconifyFun callback);

    MaximizeFun
    set_maximize_callback(MaximizeFun callback);

    FramebufferSizeFun
    set_framebuffer_size_callback(FramebufferSizeFun callback);

    ContentScaleFun
    set_content_scale_callback(ContentScaleFun callback);

    MouseButtonFun
    set_mouse_button_callback(MouseButtonFun callback);

    CursorPosFun
    set_cursor_pos_callback(CursorPosFun callback);

    CursorEnterFun
    set_cursor_enter_callback(CursorEnterFun callback);

    ScrollFun
    set_scroll_callback(ScrollFun callback);

    KeyFun
    set_key_callback(KeyFun callback);

    CharFun
    set_char_callback(CharFun callback);

    DropFun
    set_drop_callback(DropFun callback);

    void
    swap_buffers() const;

    [[nodiscard]] bool
    get_input_mode(InputMode mode) const;

    [[nodiscard]] CursorInputMode
    get_cursor_input_mode() const;

    void
    set_input_mode(InputMode mode, bool value);

    void
    set_cursor_input_mode(CursorInputMode value);

    [[nodiscard]] Action
    get_key(Key key) const;

    [[nodiscard]] Action
    get_mouse_button(Button button) const;

    [[nodiscard]] std::pair<double, double>
    get_cursor_pos() const;

    void
    set_cursor_pos(double x, double y) const;

    void
    set_clipboard_string(const std::string &str) const;

    std::string
    get_clipboard_string() const;

private:
    Window(int width, int height, const char *title, GLFWmonitor *monitor, GLFWwindow *share);

    GLFWwindow *window_ = nullptr;

    mutable std::optional<std::thread::id> current_thread_{};

    PosFun             pos_fun_{};
    SizeFun            size_fun_{};
    CloseFun           close_fun_{};
    RefreshFun         refresh_fun_{};
    FocusFun           focus_fun_{};
    IconifyFun         iconify_fun_{};
    MaximizeFun        maximize_fun_{};
    FramebufferSizeFun framebuffer_size_fun_{};
    ContentScaleFun    content_scale_fun_{};
    MouseButtonFun     mouse_button_fun_{};
    CursorPosFun       cursor_pos_fun_{};
    CursorEnterFun     cursor_enter_fun_{};
    ScrollFun          scroll_fun_{};
    KeyFun             key_fun_{};
    CharFun            char_fun_{};
    DropFun            drop_fun_{};

    static void
    hint_(int hint, int value);

    static void
    hint_(int hint, const std::string &value);
};

/*
template<WindowHint H>
requires(
    H == WindowHint::RESIZABLE || H == WindowHint::VISIBLE || H == WindowHint::DECORATED ||
    H == WindowHint::FOCUSED || H == WindowHint::AUTO_ICONIFY || H == WindowHint::FLOATING ||
    H == WindowHint::MAXIMIZED || H == WindowHint::CENTER_CURSOR ||
    H == WindowHint::TRANSPARENT_FRAMEBUFFER || H == WindowHint::FOCUS_ON_SHOW ||
    H == WindowHint::SCALE_TO_MONITOR || H == WindowHint::STEREO || H == WindowHint::SRGB_CAPABLE ||
    H == WindowHint::DOUBLEBUFFER || H == WindowHint::OPENGL_FORWARD_COMPAT ||
    H == WindowHint::OPENGL_DEBUG_CONTEXT || H == WindowHint::CONTEXT_NO_ERROR ||
    H == WindowHint::COCOA_RETINA_FRAMEBUFFER ||
    H == WindowHint::COCOA_GRAPHICS_SWITCHING)
    void
    Window::hint<H, bool>(bool value) {
        Window::hint_(static_cast<int>(H), static_cast<int>(value));
    }
};
 */

// template<>
// void
// Window::hint<WindowHint::CLIENT_API>(ClientAPI value) {
//     hint_(static_cast<int>(WindowHint::CLIENT_API), static_cast<int>(value));
// }
//  template<>
//  void
//  Window::hint<WindowHint::CONTEXT_CREATION_API>(ContextCreationAPI value) {
//      hint_(static_cast<int>(WindowHint::CONTEXT_CREATION_API), static_cast<int>(value));
//  }
//  template<>
//  void
//  Window::hint<WindowHint::OPENGL_PROFILE>(OpenGLProfile value) {
//      hint_(static_cast<int>(WindowHint::OPENGL_PROFILE), static_cast<int>(value));
//  }
//  template<>
//  void
//  Window::hint<WindowHint::CONTEXT_ROBUSTNESS>(ContextRobustness value) {
//      hint_(static_cast<int>(WindowHint::CONTEXT_ROBUSTNESS), static_cast<int>(value));
//  }
//  template<>
//  void
//  Window::hint<WindowHint::CONTEXT_RELEASE_BEHAVIOR>(ContextReleaseBehavior value) {
//      hint_(static_cast<int>(WindowHint::CONTEXT_RELEASE_BEHAVIOR), static_cast<int>(value));
//  }

double
get_time();

void
set_time(double time);

uint64_t
get_timer_value();

uint64_t
get_timer_frequency();

void
poll_events();

void
wait_events();

void
wait_events_timeout(double timeout);

void
post_empty_event();

bool
raw_mouse_motion_supported();

std::string
get_key_name(int key, int scancode);

int
get_key_scancode(Key key);

void
set_swap_interval(int interval);

bool
extension_supported(const std::string &extension);

using GLProc = void (*)();

GLProc
get_proc_address(const std::string &proc_name);

} // namespace GLFW
