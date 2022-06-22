#define CL_HPP_ENABLE_EXCEPTIONS
#define CL_HPP_TARGET_OPENCL_VERSION 300

#include "glfwcpp.hpp"
#include "render.hpp"
#include "camera.hpp"
#include "cl_struct.h"

#include <optional>
#include <atomic>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <random>

#define NOMINMAX
#include <GL/gl3w.h>
#include <fstream>

#include "glm/gtc/type_ptr.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

template<typename T>
class ThreadSafe {
public:
    ThreadSafe() = default;

    ThreadSafe(const T &val)
        : val_{val} {}

    template<typename... Args>
    explicit ThreadSafe(Args &&...args)
        : val_(std::forward<Args>(args)...) {}

    ThreadSafe &
    operator=(const T &other) {
        std::unique_lock read_lock(read_);
        val_ = other;
        return *this;
    }

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
ThreadSafe<int>                                input = 40;

bool                           fullscreen = false;
std::tuple<int, int, int, int> windowed_dim;

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

        // auto target_time = frame_start + (1.0 / 30.0);
        // while (GLFW::get_time() < target_time) {
        //     // spin until framerate is reached
        // }
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

    shared_camera.modify([](Camera &cam) {
        cam.transform.setLocalPosition({-0.34766639522035447, 0, 1.9952449076456595});
        cam.setRotation(glm::degrees(3.87637758f), glm::degrees(0.0f));
        cam.setFOV(glm::degrees(1.57079637f));
    });

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

    Object objects[] = {
        {
            // Light
            .type     = BOX,
            .box      = {.min = {-0.5f, 0.9f, -0.5f}, .max = {0.5f, 1.1f, 0.5f}},
            .color    = {255, 255, 255},
            .emission = 5,
        },
        {
            // Sphere
            .type     = SPHERE,
            .sphere   = {.center = {0, -0.7f, 0}, .radius = 0.3f},
            .color    = {255, 255, 255},
            .emission = 0,
        },
        {
            // Floor
            .type     = BOX,
            .box      = {.min = {-1, -1.05f, -1}, .max = {1, -1, 1}},
            .color    = {200, 200, 200},
            .emission = 0,
        },
        {
            // Left Wall
            .type     = BOX,
            .box      = {.min = {-1.05f, -1, -1}, .max = {-1, 1, 1}},
            .color    = {255, 100, 100},
            .emission = 0,
        },
        {
            // Back Wall
            .type     = BOX,
            .box      = {.min = {-1, -1, -1.05f}, .max = {1, 1, -1}},
            .color    = {200, 200, 200},
            .emission = 0,
        },
        {
            // Right Wall
            .type     = BOX,
            .box      = {.min = {1, -1, -1}, .max = {1.05f, 1, 1}},
            .color    = {100, 255, 100},
            .emission = 0,
        },
        {
            // Ceiling
            .type     = BOX,
            .box      = {.min = {-1, 1.05f, -1}, .max = {1, 1, 1}},
            .color    = {200, 200, 200},
            .emission = 0,
        },
    };

    std::string kernel_code;
    {
        auto file = std::ifstream("clpt/src/kernel.cl");
        file.exceptions(std::ifstream::failbit);
        auto ss = std::stringstream();
        ss << file.rdbuf();
        kernel_code = ss.str();
    }

    auto renderer = Render::Renderer(kernel_code, "my_kernel", width, height);

    renderer.addInputBuffer(1, 16 * sizeof(float));
    renderer.addInputBuffer(2, width * height * sizeof(unsigned long long));

    auto r   = std::random_device();
    auto gen = std::mt19937_64(r());
    auto dis = std::uniform_int_distribution<unsigned long long>();
    {
        std::vector<unsigned long long> seed;
        seed.reserve(width * height);
        for (int i = 0; i < width * height; i++) {
            seed.push_back(dis(gen));
        }
        renderer.writeBuffer(2, width * height * sizeof(unsigned long long), seed.data());
    }

    renderer.addInputBuffer(5, sizeof(objects));
    renderer.writeBuffer(5, sizeof(objects), objects);
    {
        unsigned int val = sizeof(objects) / sizeof(*objects);
        renderer.setKernelArg(6, sizeof(val), &val);
    }

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
            {
                int arg = input;
                renderer.setKernelArg(3, sizeof(int), &arg);
            }
            {
                auto arg = dis(gen);
                renderer.setKernelArg(4, sizeof(arg), &arg);
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
                {
                    renderer.addInputBuffer(2, width * height * sizeof(unsigned long long));

                    std::vector<unsigned long long> seed;
                    seed.reserve(width * height);
                    for (int i = 0; i < width * height; i++) {
                        seed.push_back(dis(gen));
                    }
                    renderer.writeBuffer(
                        2,
                        width * height * sizeof(unsigned long long),
                        seed.data());
                }
            }
            //            if (GLFW::get_time() - frame_start > 1.0 / framerate) {
            //                missed_frames++;
            //                std::cout << missed_frames << " / " << frame << " = "
            //                          << (double)missed_frames / frame << std::endl;
            //            }

            if (fps_limit) {
                auto target_time = frame_start + (1.0 / framerate);
                while (GLFW::get_time() < target_time) {
                    // spin until framerate is reached
                }
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

    auto monitor    = GLFW::Monitor::get_primary();
    auto video_mode = monitor.get_video_mode();

    auto window = GLFW::Window(width, height, "GLFW Window");

    window.set_framebuffer_size_callback([&](int w, int h) { frame_size = std::make_pair(w, h); });

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
                    cursor_locked = true;
                    window.set_cursor_input_mode(CursorInputMode::CURSOR_DISABLED);
                    window.set_cursor_pos(0, 0);
                }
            }
        }
    });

    window.set_key_callback(
        GLFW::Key::KEY_ESCAPE,
        true,
        [&](GLFW::Key, int, GLFW::Action action, GLFW::Modifier) {
            if (cursor_locked && action == GLFW::Action::PRESS) {
                auto [w, h]   = window.get_framebuffer_size();
                cursor_locked = false;
                window.set_cursor_input_mode(GLFW::CursorInputMode::CURSOR_NORMAL);
                window.set_cursor_pos((float)w / 2, (float)h / 2);
            }
        });

    window.set_key_callback(
        GLFW::Key::KEY_W,
        true,
        [&](GLFW::Key, int, GLFW::Action action, GLFW::Modifier) {
            if (action == GLFW::Action::PRESS) {
                shared_velocity.modify([](auto &vel) { vel.y++; });
            } else if (action == GLFW::Action::RELEASE) {
                shared_velocity.modify([](auto &vel) { vel.y--; });
            }
        });
    window.set_key_callback(
        GLFW::Key::KEY_S,
        true,
        [&](GLFW::Key, int, GLFW::Action action, GLFW::Modifier) {
            if (action == GLFW::Action::PRESS) {
                shared_velocity.modify([](auto &vel) { vel.y--; });
            } else if (action == GLFW::Action::RELEASE) {
                shared_velocity.modify([](auto &vel) { vel.y++; });
            }
        });
    window.set_key_callback(
        GLFW::Key::KEY_D,
        true,
        [&](GLFW::Key, int, GLFW::Action action, GLFW::Modifier) {
            if (action == GLFW::Action::PRESS) {
                shared_velocity.modify([](auto &vel) { vel.x++; });
            } else if (action == GLFW::Action::RELEASE) {
                shared_velocity.modify([](auto &vel) { vel.x--; });
            }
        });
    window.set_key_callback(
        GLFW::Key::KEY_A,
        true,
        [&](GLFW::Key, int, GLFW::Action action, GLFW::Modifier) {
            if (action == GLFW::Action::PRESS) {
                shared_velocity.modify([](auto &vel) { vel.x--; });
            } else if (action == GLFW::Action::RELEASE) {
                shared_velocity.modify([](auto &vel) { vel.x++; });
            }
        });

    window.set_key_callback(
        GLFW::Key::KEY_F,
        true,
        [&](GLFW::Key, int, GLFW::Action action, GLFW::Modifier) {
            if (action == GLFW::Action::PRESS) {
                if (fullscreen) {
                    auto [x, y, w, h] = windowed_dim;
                    window.set_windowed(x, y, w, h);
                } else {
                    windowed_dim = std::tuple_cat(window.get_pos(), window.get_size());
                    window.set_monitor(
                        monitor,
                        video_mode.width,
                        video_mode.height,
                        video_mode.refresh_rate);
                }
                fullscreen = !fullscreen;
            }
        });

    window.set_key_callback(
        GLFW::Key::KEY_Q,
        true,
        [&](GLFW::Key, int, GLFW::Action action, GLFW::Modifier) {
            if (action == GLFW::Action::PRESS) {
                input.modify([](int &val) { val--; });
            }
        });
    window.set_key_callback(
        GLFW::Key::KEY_E,
        true,
        [&](GLFW::Key, int, GLFW::Action action, GLFW::Modifier) {
            if (action == GLFW::Action::PRESS) {
                input.modify([](int &val) { val++; });
            }
        });

    window.set_scroll_callback([&](double, double y) {
        float old_fov, new_fov;
        shared_camera.modify([&](auto &camera) {
            old_fov     = camera.getFOV();
            auto factor = old_fov / 180.0f;
            factor      = -factor / (factor - 1);
            new_fov     = factor * pow(0.9f, static_cast<float>(y));
            new_fov     = new_fov / (new_fov + 1);
            new_fov *= 180.0f;
            camera.setFOV(new_fov);
        });
        sensitivity.modify([&](glm::vec2 &sens) {
            sens *= glm::tan(glm::radians(new_fov) / 2) / glm::tan(glm::radians(old_fov) / 2);
        });
    });
    window.set_cursor_input_mode(GLFW::CursorInputMode::CURSOR_DISABLED);
    window.set_cursor_pos(0, 0);
    if (GLFW::raw_mouse_motion_supported()) {
        window.set_input_mode(GLFW::InputMode::RAW_MOUSE_MOTION, true);
    }

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
