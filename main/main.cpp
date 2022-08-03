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
#include <fstream>
#include <numeric>

#define NOMINMAX
#include <GL/gl3w.h>

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
ThreadSafe<bool>                               cursor_locked = false;
ThreadSafe<glm::vec2>                          shared_velocity;
ThreadSafe<glm::vec2>                          sensitivity{1, 1};
ThreadSafe<int>                                input = 10;

bool                           fullscreen = false;
std::tuple<int, int, int, int> windowed_dim;

static inline cl_float3
min(const cl_float3 &a, const cl_float3 &b) {
    return {
        std::min(a.x, b.x),
        std::min(a.y, b.y),
        std::min(a.z, b.z),
    };
}

static inline cl_float3
max(const cl_float3 &a, const cl_float3 &b) {
    return {
        std::max(a.x, b.x),
        std::max(a.y, b.y),
        std::max(a.z, b.z),
    };
}

static inline cl_float3
operator+(const cl_float3 &vec, float s) {
    return {
        vec.x + s,
        vec.y + s,
        vec.z + s,
    };
}

static inline cl_float3
operator+(const cl_float3 &a, const cl_float3 &b) {
    return {
        a.x + b.x,
        a.y + b.y,
        a.z + b.z,
    };
}

static inline cl_float3
operator-(const cl_float3 &vec, float s) {
    return {
        vec.x - s,
        vec.y - s,
        vec.z - s,
    };
}

static inline cl_float3
operator-(const cl_float3 &a, const cl_float3 &b) {
    return {
        a.x - b.x,
        a.y - b.y,
        a.z - b.z,
    };
}

static inline cl_float3
operator/(const cl_float3 &vec, float s) {
    return {
        vec.x / s,
        vec.y / s,
        vec.z / s,
    };
}

static std::pair<cl_float3, cl_float3>
object_bounds(const Object &obj) {
    switch (obj.type) {
        case Type::BOX:
            return {
                min(obj.box.min, obj.box.max),
                max(obj.box.min, obj.box.max),
            };
        case Type::SPHERE:
            return {
                obj.sphere.center - obj.sphere.radius,
                obj.sphere.center + obj.sphere.radius,
            };
    }
    return {}; // silence compiler warning, all paths should be covered above.
}

float
kd_metric(
    float                                       lower_bound,
    float                                       upper_bound,
    const std::vector<std::pair<float, float>> &bounds,
    float                                       split) {
    (void)lower_bound;
    (void)upper_bound;
    (void)bounds;
    (void)split;
    auto total = bounds.size();
    int  below = 0, above = 0;
    for (auto &[l, u] : bounds) {
        if (l < split) below++;
        if (u > split) above++;
    }
    if (above == 0) {
        return (upper_bound - split) / (upper_bound - lower_bound);
    }
    if (below == 0) {
        return (split - lower_bound) / (upper_bound - lower_bound);
    }
    return glm::sin(glm::pi<float>() * (float)below / (float)total) *
           glm::sin(glm::pi<float>() * (float)above / (float)total);
}

void
generate_kd(
    cl_float3                                           lower_bound,
    cl_float3                                           upper_bound,
    int                                                 depth,
    std::vector<cl_uint>                              &&indices,
    const std::vector<std::pair<cl_float3, cl_float3>> &bounds,
    std::vector<KDTreeNode>                            &out_nodes,
    std::vector<cl_uint>                               &out_indices) {
    //    for (auto i : indices) {
    //        assert(bounds[i].first.x >= lower_bound.x);
    //        assert(bounds[i].first.y >= lower_bound.y);
    //        assert(bounds[i].first.z >= lower_bound.z);
    //        assert(bounds[i].second.x <= upper_bound.x);
    //        assert(bounds[i].second.y <= upper_bound.y);
    //        assert(bounds[i].second.z <= upper_bound.z);
    //    }
    out_nodes.emplace_back();
    auto &node       = out_nodes.back();
    node.lower_bound = lower_bound;
    node.upper_bound = upper_bound;

    if (depth >= MAX_KD_DEPTH || indices.empty()) {
        node.flags = 3;
        node.nprims |= (indices.size() << 2);
        if (indices.empty()) {
            node.one_prim = 0;
        } else if (indices.size() == 1) {
            node.one_prim = indices.front();
        } else {
            node.prim_ids_offset = static_cast<cl_uint>(out_indices.size());
            out_indices.insert(out_indices.end(), indices.begin(), indices.end());
        }

        return;
    }
    /*
    int   best_axis    = -1;
    float best_split   = 0;
    int   best_overlap = 0;
    for (int axis = 0; axis < 3; axis++) {
        std::vector<float> points;
        points.reserve(2 * indices.size());
        for (auto i : indices) {
            auto &[lb, ub] = bounds[i];
            auto l = lb.s[axis], u = ub.s[axis];
            points.push_back(l);
            points.push_back(u);
        }
        std::transform(indices.begin(), indices.end(), std::back_inserter(points), [&](cl_uint i) {
            auto &[lb, ub] = bounds[i];
            auto mid       = (lb + ub) / 2;
            return mid.s[axis];
        });
        std::sort(points.begin(), points.end());
        float mid     = *(points.begin() + points.size() / 2);
        int   overlap = 0;
        for (auto i : indices) {
            auto &[lb, ub] = bounds[i];
            auto l = lb.s[axis], u = ub.s[axis];
            if (l <= mid && u >= mid) {
                overlap++;
            }
        }
        if (best_axis == -1 || overlap < best_overlap) {
            best_axis    = axis;
            best_split   = mid;
            best_overlap = overlap;
        }
    }
    auto axis  = best_axis;
    auto split = best_split;
     */

    //    int   best_axis = -1;
    //    float best_vol  = 0;
    /*
    auto ext = upper_bound - lower_bound;

    for (int axis = 0; axis < 3; axis++) {
        float min_b = 0, max_b = 0;
        for (int i = 0; i < indices.size(); i++) {
            auto id       = indices[i];
            auto [lb, ub] = bounds[id];
            auto l        = lb.s[axis];
            auto u        = ub.s[axis];
            if (i == 0 || l < min_b) min_b = l;
            if (i == 0 || u > max_b) max_b = u;
        }

        if (min_b > lower_bound.s[axis]) {
            float vol = (min_b - lower_bound.s[axis]);
            for (int a = 0; a < 3; a++) {
                if (a != axis) vol *= ext.s[a];
            }
            std::cout << min_b << " " << vol << std::endl;
        }
        if (max_b < upper_bound.s[axis]) {
            float vol = (upper_bound.s[axis] - max_b);
            for (int a = 0; a < 3; a++) {
                if (a != axis) vol *= ext.s[a];
            }
            std::cout << max_b << " " << vol << std::endl;
        }
    }

    int axis = ext.x > ext.y ? (ext.x > ext.z ? 0 : 2) : (ext.y > ext.z ? 1 : 2);

    float split = 0;
    {
        std::vector<float> pts;
        pts.reserve(2 * indices.size());
        for (auto id : indices) {
            auto [lb, ub] = bounds[id];
            auto l        = lb.s[axis] ;// - 0.0001f;
            auto u        = ub.s[axis] ;// + 0.0001f;
            if (l >= lower_bound.s[axis]) pts.push_back(l);
            if (u <= upper_bound.s[axis]) pts.push_back(u);
        }
        std::sort(pts.begin(), pts.end());
        float mid = *(pts.begin() + pts.size() / 2);
        float lb  = lower_bound.s[axis];
        float ub  = upper_bound.s[axis];
        split     = std::clamp(mid, lb, ub);
    }
    */

    // float split = (lower_bound.s[axis] + upper_bound.s[axis]) / 2;

    float best_metric = 0;
    int   best_axis   = 0;
    float best_split  = 0;

    //    auto r    = std::random_device();
    //    auto gen  = std::mt19937_64(r());
    //    auto dist = std::uniform_real_distribution<float>(0, 1);

    for (int axis = 0; axis < 3; axis++) {
        std::vector<std::pair<float, float>> axis_bounds;
        axis_bounds.reserve(indices.size());
        for (auto id : indices) {
            auto &[lb, ub] = bounds[id];
            axis_bounds.emplace_back(lb.s[axis], ub.s[axis]);
        }

        float l = lower_bound.s[axis];
        float u = upper_bound.s[axis];
        for (auto splits : axis_bounds) {
            //            float split  = l + dist(gen) * (u - l);
            auto [s1, s2] = splits;
            {
                float metric = kd_metric(l, u, axis_bounds, s1);
                if (metric > best_metric) {
                    best_metric = metric;
                    best_axis   = axis;
                    best_split  = s1;
                }
            }
            {
                float metric = kd_metric(l, u, axis_bounds, s2);
                if (metric > best_metric) {
                    best_metric = metric;
                    best_axis   = axis;
                    best_split  = s2;
                }
            }
        }
    }

    node.split = best_split;
    node.flags = best_axis;

    std::vector<cl_uint> lower_ids, upper_ids;
    for (auto i : indices) {
        auto &[lb, ub] = bounds[i];
        if (lb.s[best_axis] < best_split) {
            lower_ids.push_back(i);
        }
        if (ub.s[best_axis] > best_split) {
            upper_ids.push_back(i);
        }
    }
    auto node_id = out_nodes.size() - 1;
    {
        auto upper         = upper_bound;
        upper.s[best_axis] = best_split;
        generate_kd(
            lower_bound,
            upper,
            depth + 1,
            std::move(lower_ids),
            bounds,
            out_nodes,
            out_indices);
    }
    out_nodes[node_id].above_child |= (out_nodes.size() << 2);
    {
        auto lower         = lower_bound;
        lower.s[best_axis] = best_split;
        generate_kd(
            lower,
            upper_bound,
            depth + 1,
            std::move(upper_ids),
            bounds,
            out_nodes,
            out_indices);
    }
}

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
        //        cam.transform.setLocalPosition({0, 0.3f, -1.6f});
        //        cam.setRotation(0, 30.0f);
        cam.transform.setLocalPosition({0, 0, 3});
        cam.setFOV(45);

        // cam.transform.setLocalPosition({-0.34766639522035447, 0, 1.9952449076456595});
        // cam.setRotation(glm::degrees(3.87637758f), glm::degrees(0.0f));
        // cam.setFOV(glm::degrees(1.57079637f));
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

    std::vector<Object> objects{
        /*
        {
            .type  = BOX,
            .box   = {.min = {-0.5f, 0.0f, -0.5f}, .max = {0.5f, 0.0f, 0.5f}},
            .color = {255, 255, 255},
        }
         */
        /*
        {
            // Sphere
            .type     = SPHERE,
            .sphere   = {.center = {-0.5f, -0.5f, -0.4f}, .radius = 0.51f},
            .color    = {200, 200, 200},
            .specularity = 0.0f,
            .IOR      = 1.0f,
        },
        {
            // Sphere
            .type      = SPHERE,
            .sphere    = {.center = {0.5f, -0.5f, -0.4f}, .radius = 0.51f},
            .color     = {255, 255, 255},
            .specularity  = 0.9f,
            .roughness = 0.0f,
            .IOR       = 1.0f,
        },
         */
        {
            // Floor
            .type   = BOX,
            .box    = {.min = {-1, -1, -1}, .max = {1, -1, 1}},
            .albedo = {0.8f, 0.8f, 0.8f},
        },

        {
            // Left Wall
            .type   = BOX,
            .box    = {.min = {-1, -1, -1}, .max = {-1, 1, 1}},
            .albedo = {0.8f, 0.4f, 0.4f},
        },
        {
            // Back Wall
            .type   = BOX,
            .box    = {.min = {-1, -1, -1}, .max = {1, 1, -1}},
            .albedo = {0.8f, 0.8f, 0.8f},
        },
        {
            // Right Wall
            .type   = BOX,
            .box    = {.min = {1, -1, -1}, .max = {1, 1, 1}},
            .albedo = {0.4f, 0.8f, 0.4f},
        },
        {
            .type     = SPHERE,
            .sphere   = {.center = {0, 1.5f, 0}, .radius = 0.5f},
            .albedo   = {1, 1, 1},
            .emission = {10, 10, 10},
        },

        /*
        {
            // Front Wall
            .type   = BOX,
            .box    = {.min = {-1, -1, 1}, .max = {1, 1, 1.05f}},
            .albedo = {0.8f, 0.8f, 0.8f},
        },
         */
        /*
        {
            // Ceiling
            .type  = BOX,
            .box   = {.min = {-1, 1.05f, -1}, .max = {1, 1, 1}},
            .color = {255, 255, 255},
        },
         */
    };

    // TODO what if there are no objects?
    std::vector<std::pair<cl_float3, cl_float3>> bounds;
    bounds.reserve(objects.size());
    for (auto &o : objects) {
        bounds.push_back(object_bounds(o));
    }

    auto [lb, ub] = bounds.front();
    for (auto it = bounds.begin() + 1; it != bounds.end(); it++) {
        lb = min(lb, it->first);
        ub = max(ub, it->second);
    }

    std::vector<cl_uint> indices(bounds.size());
    std::iota(indices.begin(), indices.end(), 0);

    std::vector<KDTreeNode> nodes;
    std::vector<cl_uint>    prim_ids;

    generate_kd(lb, ub, 0, std::move(indices), bounds, nodes, prim_ids);

    std::string kernel_code;
    {
        auto file = std::ifstream("clpt/src/kernel.cl");
        file.exceptions(std::ifstream::failbit);
        auto ss = std::stringstream();
        ss << file.rdbuf();
        kernel_code = ss.str();
    }

    auto renderer = Render::Renderer(kernel_code, "my_kernel", width, height);

    renderer.addInputBuffer(1, 16 * sizeof(float)); // Camera

    renderer.addInputBuffer(2, width * height * sizeof(unsigned long long)); // Random Seed

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

    if (!objects.empty()) {
        renderer.addInputBuffer(5, objects.size() * sizeof(Object));
        renderer.writeBuffer(5, objects.size() * sizeof(Object), objects.data());
    } else {
        renderer.setKernelArg(5, 0, nullptr);
    }
    {
        auto val = static_cast<unsigned int>(objects.size());
        renderer.setKernelArg(6, sizeof(val), &val);
    }

    auto lights = std::vector<Light>();
    for (size_t i = 0; i < objects.size(); i++) {
        auto &obj = objects[i];
        if (obj.emission.x > 0 || obj.emission.y > 0 || obj.emission.z > 0) {
            lights.emplace_back(Light{
                .index = static_cast<cl_uint>(i),
            });
        }
    }

    if (!lights.empty()) {
        renderer.addInputBuffer(7, lights.size() * sizeof(Light));
        renderer.writeBuffer(7, lights.size() * sizeof(Light), lights.data());
    } else {
        renderer.setKernelArg(7, 0, nullptr);
    }
    {
        auto val = static_cast<unsigned int>(lights.size());
        renderer.setKernelArg(8, sizeof(val), &val);
    }

    renderer.setKernelArg(9, sizeof(lb), &lb);
    renderer.setKernelArg(10, sizeof(ub), &ub);

    renderer.addInputBuffer(11, nodes.size() * sizeof(*nodes.data()));
    renderer.writeBuffer(11, nodes.size() * sizeof(*nodes.data()), nodes.data());

    renderer.addInputBuffer(12, prim_ids.size() * sizeof(*prim_ids.data()));
    renderer.writeBuffer(12, prim_ids.size() * sizeof(*prim_ids.data()), prim_ids.data());

    int frame = 0;

    std::vector<float> frames(120, 0);

    auto last_time  = GLFW::get_time();
    auto last_frame = 0;
    auto fps_limit  = true;
    auto framerate  = (float)refresh_rate;
    try {
        while (!window.should_close()) {
            auto frame_start = GLFW::get_time();

            //            objects[0].sphere.center.y = -0.2f;
            //            objects[0].sphere.center.x = (cl_float)sin(GLFW::get_time()) / 2;
            //            objects[0].sphere.center.z = (cl_float)cos(GLFW::get_time()) / 2;
            //            renderer.writeBuffer(5, sizeof(objects), objects);

            //            objects[1].IOR = 2 + (cl_float)cos(GLFW::get_time());
            //            renderer.writeBuffer(5, objects.size() * sizeof(Object), objects.data());

            //            lights[0].object.sphere.center = {
            //                (float)cos(GLFW::get_time()) / 10,
            //                1.0f,
            //                (float)sin(GLFW::get_time()) / 10,
            //            };
            //            lights[1].object.sphere.center = {
            //                (float)cos(GLFW::get_time() + 2 * glm::pi<float>() / 3),
            //                0.0f,
            //                (float)sin(GLFW::get_time() + 2 * glm::pi<float>() / 3),
            //            };
            //            lights[2].object.sphere.center = {
            //                (float)cos(GLFW::get_time() - 2 * glm::pi<float>() / 3) / 10,
            //                1.0f,
            //                (float)sin(GLFW::get_time() - 2 * glm::pi<float>() / 3) / 10,
            //            };
            //            renderer.writeBuffer(7, lights.size() * sizeof(Light), lights.data());

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
            ImGui::SetNextWindowPos({0, 0});
            if (ImGui::Begin(
                    "Frame Time",
                    nullptr,
                    ImGuiWindowFlags_::ImGuiWindowFlags_AlwaysAutoResize)) {
                auto [min, max] = std::minmax_element(frames.begin(), frames.end());
                auto label      = std::to_string(frames.back()) +
                             " ms\n"
                             "Max: " +
                             std::to_string(*max) + " ms\nMin: " + std::to_string(*min) + " ms";
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

            if (ImGui::Begin(
                    "SPP",
                    nullptr,
                    ImGuiWindowFlags_::ImGuiWindowFlags_AlwaysAutoResize)) {
                int val = input;
                if (ImGui::InputInt("SPP", &val)) {
                    input = val;
                }
            }
            ImGui::End();

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            window.swap_buffers();

            auto curr_time = GLFW::get_time();
            auto time_diff = curr_time - last_time;
            if (time_diff > 0.05) {
                // auto fps = (frame - last_frame) / time_diff;
                auto frame_time = 1000.0f * time_diff / (frame - last_frame);
                if (frames.size() >= 120) {
                    frames.erase(frames.begin());
                }
                frames.push_back(static_cast<float>(frame_time));

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

    int width = 1024, height = 1024;

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
    /*
    window.set_cursor_input_mode(GLFW::CursorInputMode::CURSOR_DISABLED);
    window.set_cursor_pos(0, 0);
    cursor_locked = true;
     */
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
