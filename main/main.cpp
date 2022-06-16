#define CL_HPP_ENABLE_EXCEPTIONS
#define CL_HPP_TARGET_OPENCL_VERSION 200

#include "glfwcpp.hpp"
#include "render.hpp"
#include "camera.hpp"

#include <optional>
#include <atomic>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <thread>
#include <mutex>
#include <shared_mutex>

#define NOMINMAX
#include <GL/gl3w.h>

#include "glm/gtc/type_ptr.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

static const char *kernel_code = R"(
float3
mul(float4 *M, float3 X) {
    return (float3){
        dot(M[0].xyz, X) + M[0].w,
        dot(M[1].xyz, X) + M[1].w,
        dot(M[2].xyz, X) + M[2].w,
    } / (dot(M[3].xyz, X) + M[3].w);
}

bool
ray_sphere_intersect(float3 origin, float3 dir, float3 center, float radius, float *t) {
    float3 m = origin - center;
    float b = dot(m, dir);
    float c = dot(m, m) - radius * radius;
    if (c > 0.0f && b > 0.0f) return false;
    float d = b*b - c;

    if (d < 0.0f) return false;

    *t = -b - sqrt(d);
    // if (*t < 0.0f) *t = 0.0f;
    return true;
}

void
kernel my_kernel(
        __write_only image2d_t output,
        __global __read_only float4 *camera
){
    int2 coord = (int2)(get_global_id(0), get_global_id(1));
    int2 res   = get_image_dim(output);

    float2 ratio = 2*(convert_float2(coord) / convert_float2(res)) - 1;
    float3 pos = (float3) {
        camera[0].z,
        camera[1].z,
        camera[2].z
    } / camera[3].z;
    float3 fcp = mul(camera, (float3)(ratio,  1.0f));
    float3 dir = normalize((fcp - pos).xyz);
    float3 color = 1.0f;
    float t;
    float3 center = (float3)0.0f;
    if (ray_sphere_intersect(pos, dir, center, 1.0f, &t)) {
        float3 h = pos + dir * t;
        float3 n = normalize(h - center);
        dir -= 2*dot(dir, n)*n;
        color *= (float3)(1, 0, 0);
    }
    color *= (dir + 1) / 2;

    float a = atan2(dir.z, dir.x) + M_PI;
    float b = atan2(dir.y, sqrt(dir.x * dir.x + dir.z * dir.z)) + M_PI;
    if (fmod(a, (float)M_PI/20.0f) < 0.01f || fmod(b, (float)M_PI/20.0f) < 0.01f) {
        color = 0;
    }
    write_imagef(output, coord, (float4)(color, 1.0f));
}
)";

template<typename T>
class ThreadSafe {
public:
    ThreadSafe() = default;

    ThreadSafe(const T &val)
        : val_{val} {}

    template<typename... Args>
    ThreadSafe(Args &&...args)
        : val_(std::forward<Args>(args)...) {}

    [[nodiscard]] T
    get() const {
        std::shared_lock lock(read_);
        return val_;
    }
    [[nodiscard]] operator T() const {
        return get();
    }

    void
    modify(std::function<void(T &)> func) {
        std::scoped_lock write_lock(write_);
        auto             copy = val_;
        func(copy);
        std::unique_lock read_lock(read_);
        val_ = copy;
    }

private:
    T                         val_{};
    mutable std::shared_mutex read_{};
    std::mutex                write_{};
};

ThreadSafe<std::optional<std::pair<int, int>>> frame_size;
ThreadSafe<Camera>                             shared_camera;
ThreadSafe<bool>                               cursor_locked = true;
ThreadSafe<glm::vec2>                          shared_velocity;
ThreadSafe<glm::vec2>                          sensitivity{1, 1};

[[maybe_unused]] static void
physics_loop(const GLFW::Window &window, int) {
    auto last_frame_start = GLFW::get_time();
    while (!window.should_close()) {
        auto frame_start = GLFW::get_time();
        auto delta_time  = frame_start - last_frame_start;
        last_frame_start = frame_start;

        auto velocity = shared_velocity.get();
        if (velocity.x != 0 || velocity.y != 0) {
            shared_camera.modify([&](Camera &camera) {
                auto pos     = camera.transform.localPosition();
                auto forward = camera.forward();
                auto right   = camera.right();
                pos += static_cast<float>(delta_time) * (velocity.x * right + velocity.y * forward);
                camera.transform.setLocalPosition(pos);
            });
        }

        auto target_time = frame_start + (1.0 / 30.0);
        while (GLFW::get_time() < target_time) {
            // spin until framerate is reached
        }
    }
}

[[maybe_unused]] static void
render_loop(
    const GLFW::Window &window,
    const char         *glsl_version,
    int                 width,
    int                 height,
    int                 refresh_rate) {
    window.make_context_current();

    GLFW::set_swap_interval(0);

    if (gl3wInit()) {
        fprintf(stderr, "failed to initialize OpenGL\n");
        exit(EXIT_FAILURE);
    }
    if (!gl3wIsSupported(3, 2)) {
        fprintf(stderr, "OpenGL 3.2 not supported\n");
        exit(EXIT_FAILURE);
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io    = ImGui::GetIO();
    io.IniFilename = nullptr;
    (void)io;
    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window(), true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    auto renderer = Render::Renderer(kernel_code, "my_kernel", width, height);

    renderer.addInputBuffer(1, 16 * sizeof(float));

    int frame = 0;

    std::vector<float> frames(120, 0);

    auto last_time  = GLFW::get_time();
    auto last_frame = 0;
    auto fps_limit  = true;
    auto framerate  = (float)refresh_rate;
    try {
        while (!window.should_close()) {
            auto frame_start = GLFW::get_time();
            frame++;

            {
                auto camera = shared_camera.get();
                auto cam_mat =
                    glm::transpose(glm::inverse(camera.getMatrix((float)width / (float)height)));
                auto cam_data = glm::value_ptr(cam_mat);

                renderer.writeBuffer(1, 16 * sizeof(float), cam_data);
            }

            //  Render the frame with OpenCL then draw it
            renderer.render();

            // Start the Dear ImGui frame
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            if (ImGui::Begin(
                    "Framerate",
                    nullptr,
                    ImGuiWindowFlags_::ImGuiWindowFlags_AlwaysAutoResize)) {
                auto [min, max] = std::minmax_element(frames.begin(), frames.end());
                auto label      = std::to_string((int)ImGui::GetIO().Framerate) +
                             " fps\n"
                             "Min: " +
                             std::to_string((int)*min) +
                             " fps\n"
                             "Max: " +
                             std::to_string((int)*max) + " fps";
                ImGui::PlotLines(
                    label.c_str(),
                    frames.data(),
                    (int)frames.size(),
                    0,
                    nullptr,
                    0.0f);
                ImGui::Checkbox("Limit FPS", &fps_limit);
                ImGui::SliderFloat("Max FPS", &framerate, 15.0f, (float)refresh_rate);
            }
            ImGui::End();

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            window.swap_buffers();

            if (fps_limit) {
                auto target_time = frame_start + (1.0 / framerate);
                while (GLFW::get_time() < target_time) {
                    // spin until framerate is reached
                }
            }

            auto curr_time = GLFW::get_time();
            auto time_diff = curr_time - last_time;
            if (time_diff > 0.05) {
                auto fps = (frame - last_frame) / time_diff;
                // auto frame_time = time_diff / (frame - last_frame);
                if (frames.size() >= 120) {
                    frames.erase(frames.begin());
                }
                frames.push_back(static_cast<float>(fps));

                last_frame = frame;
                last_time  = curr_time;
            }

            if (frame_size.get()) {
                frame_size.modify([&](auto &opt_size) {
                    auto [w, h] = opt_size.value();
                    opt_size.reset();
                    width  = w;
                    height = h;
                });
                renderer.resize(width, height);
            }
        }
    } catch (std::exception &e) {
        std::cerr << e.what();
        window.set_should_close(true);
    }

    ImGui::DestroyContext();
    GLFW::Window::clear_current_context();
}

int
main() try {
    GLFW::Init::initialize();

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char *glsl_version = "#version 100";
    GLFW::Window::hint<GLFW::WindowHint::CONTEXT_VERSION_MAJOR>(2);
    GLFW::Window::hint<GLFW::WindowHint::CONTEXT_VERSION_MINOR>(0);
    GLFW::Window::hint<GLFW::WindowHint::CLIENT_API>(GLFW::ClientAPI::OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char *glsl_version = "#version 150";
    GLFW::Window::hint<GLFW::WindowHint::CONTEXT_VERSION_MAJOR>(3);
    GLFW::Window::hint<GLFW::WindowHint::CONTEXT_VERSION_MINOR>(2);
    GLFW::Window::hint<GLFW::WindowHint::OPENGL_PROFILE>(GLFW::OpenGLProfile::OPENGL_CORE_PROFILE);
    GLFW::Window::hint<GLFW::WindowHint::OPENGL_FORWARD_COMPAT>(true);
#else
    // GL 3.0 + GLSL 130
    const char *glsl_version = "#version 130";
    GLFW::Window::hint<GLFW::WindowHint::CONTEXT_VERSION_MAJOR>(3);
    GLFW::Window::hint<GLFW::WindowHint::CONTEXT_VERSION_MINOR>(0);
    // GLFW::Window::hint<GLFW::WindowHint::OPENGL_PROFILE>(
    //     GLFW::OpenGLProfile::OPENGL_CORE_PROFILE);                     // 3.2+ only
    // GLFW::Window::hint<GLFW::WindowHint::OPENGL_FORWARD_COMPAT>(true); // 3.0+only
#endif

    int width = 1280, height = 720;

    auto monitor = GLFW::Monitor::get_primary();

    auto window = GLFW::Window(width, height, "GLFW Window");

    window.set_framebuffer_size_callback([&](int w, int h) {
        frame_size.modify([&](auto &frame) { frame = {w, h}; });
    });

    window.set_cursor_pos_callback([&](double x, double y) {
        if (cursor_locked) {
            shared_camera.modify([&](auto &camera) {
                auto sens = sensitivity.get();
                camera.addRotation(static_cast<float>(x) * sens.x, -static_cast<float>(y) * sens.y);
            });
            window.set_cursor_pos(0, 0);
        }
    });
    window.set_mouse_button_callback([&](GLFW::Button, GLFW::Action action, GLFW::Modifier) {
        using namespace GLFW;
        static ImGuiIO &io = ImGui::GetIO();
        if (!io.WantCaptureMouse) {
            if (!cursor_locked) {
                if (action == Action::PRESS) {
                    cursor_locked.modify([](bool &b) { b = true; });
                    window.set_cursor_input_mode(CursorInputMode::CURSOR_DISABLED);
                    window.set_cursor_pos(0, 0);
                }
            }
        }
    });
    window.set_key_callback([&](GLFW::Key key, int, GLFW::Action action, GLFW::Modifier) {
        using namespace GLFW;
        if (cursor_locked) {
            if (key == Key::KEY_ESCAPE && action == Action::PRESS) {
                auto [w, h] = window.get_framebuffer_size();
                cursor_locked.modify([](bool &b) { b = false; });
                window.set_cursor_input_mode(CursorInputMode::CURSOR_NORMAL);
                window.set_cursor_pos((float)w / 2, (float)h / 2);
            }
            shared_velocity.modify([&](auto &velocity) {
                if (key == Key::KEY_W) {
                    switch (action) {
                        case Action::PRESS: velocity.y++; break;
                        case Action::RELEASE: velocity.y--; break;
                        default: break;
                    }
                } else if (key == Key::KEY_D) {
                    switch (action) {
                        case Action::PRESS: velocity.x++; break;
                        case Action::RELEASE: velocity.x--; break;
                        default: break;
                    }
                } else if (key == Key::KEY_S) {
                    switch (action) {
                        case Action::PRESS: velocity.y--; break;
                        case Action::RELEASE: velocity.y++; break;
                        default: break;
                    }
                } else if (key == Key::KEY_A) {
                    switch (action) {
                        case Action::PRESS: velocity.x--; break;
                        case Action::RELEASE: velocity.x++; break;
                        default: break;
                    }
                }
            });
        }
    });
    window.set_scroll_callback([&](double, double y) {
        shared_camera.modify([&](auto &camera) {
            auto fov     = camera.getFOV() / 180.0f;
            fov          = -fov / (fov - 1);
            auto factor  = pow(0.9f, static_cast<float>(y));
            auto new_fov = fov * factor;
            new_fov      = new_fov / (new_fov + 1);
            camera.setFOV(new_fov * 180.0f);
        });
    });
    window.set_cursor_input_mode(GLFW::CursorInputMode::CURSOR_DISABLED);
    window.set_cursor_pos(0, 0);
    if (GLFW::raw_mouse_motion_supported()) {
        window.set_input_mode(GLFW::InputMode::RAW_MOUSE_MOTION, true);
    }

    auto video_mode = monitor.get_video_mode();

    shared_camera.modify([](auto &camera) { camera.transform.setPosition({0, 0, 5}); });

    auto physics_thread = std::thread(physics_loop, std::ref(window), video_mode.refresh_rate);
    auto render_thread  = std::thread(
        render_loop,
        std::ref(window),
        glsl_version,
        width,
        height,
        video_mode.refresh_rate);

    while (!window.should_close()) {
        GLFW::wait_events();
    }

    render_thread.join();
    physics_thread.join();

    return 0;
} catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
    exit(EXIT_FAILURE);
}
