#define CL_HPP_ENABLE_EXCEPTIONS
#define CL_HPP_TARGET_OPENCL_VERSION 200

#define NOMINMAX
#include <GL/gl3w.h>
#include <CL/opencl.hpp>
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#include <iostream>
#include <algorithm>

#include "render.hpp"

using user_ptr = struct {
    int width;
    int height;
};
using user_ptr_t = std::atomic<std::optional<user_ptr>>;

static void
glfw_error_callback(int err, const char *description) {
    fprintf(stderr, "Glfw Error %d: %s\n", err, description);
}

static void
glfw_framebuffer_size_callback(GLFWwindow *window, int width, int height) {
    auto &new_window_dim = *(user_ptr_t *)glfwGetWindowUserPointer(window);
    new_window_dim       = user_ptr{.width = width, .height = height};
}

static void
render_loop(GLFWwindow *window, int width, int height, float framerate) {
    user_ptr_t new_window_dim;

    glfwMakeContextCurrent(window);
    glfwSetWindowUserPointer(window, &new_window_dim);

    glfwSwapInterval(0);

    if (gl3wInit()) {
        fprintf(stderr, "failed to initialize OpenGL\n");
        exit(EXIT_FAILURE);
    }
    if (!gl3wIsSupported(3, 2)) {
        fprintf(stderr, "OpenGL 3.2 not supported\n");
        exit(EXIT_FAILURE);
    }

    std::string kernel_code = R"(
void kernel my_kernel(__write_only image2d_t output, __read_only int frame){
    int2 coord = (int2)(get_global_id(0), get_global_id(1));
    int i = coord.x + coord.y * get_image_width(output);
    float3 color = (float3) {
        ((coord.x >> 4) & 0xFF) / 255.0,
        (((coord.x & 0xF) << 4) + ((coord.y >> 8) & 0xF) ) / 255.0,
        (coord.y & 0xFF) / 255.0,
    };
//    float3 color = (float3){
//        (i / 256 / 256) % 256 / 255.0,
//        (i / 256) % 256 / 255.0,
//        i % 256 / 255.0,
//    };
//    float3 color = (float3){
//        i % 2 / 1.0,
//        i / 2 % 2 / 1.0,
//        i / 4 % 2 / 1.0,
//    };
    write_imagef(output, coord, (float4)(color, 1.0));
})";

    auto renderer = Render::Renderer(kernel_code, "my_kernel", width, height);

    int frame = 0;

    auto last_time  = glfwGetTime();
    int  last_frame = 0;
    try {
        while (!glfwWindowShouldClose(window)) {
            auto frame_start = glfwGetTime();
            frame++;
            if (new_window_dim.load()) {
                auto new_dim = new_window_dim.exchange(std::nullopt).value();

                width  = new_dim.width;
                height = new_dim.height;

                renderer.resize(width, height);
            }

            // Run OpenCL to render to texture
            renderer.setKernelArg(1, frame);
            renderer.render();

            glfwSwapBuffers(window);

            auto target_time = frame_start + (1.0f / framerate);
            while (glfwGetTime() < target_time) {
                // spin until framerate is reached
            }

            auto curr_time = glfwGetTime();
            auto time_diff = curr_time - last_time;
            if (time_diff > 1.0) {
                std::cout << (frame - last_frame) / time_diff << std::endl;
                last_frame = frame;
                last_time  = curr_time;
            }
        }
    } catch (cl::Error &e) {
        std::cerr << "OpenCL Error " << e.err() << std::endl;
        glfwSetWindowShouldClose(window, 1);
    }

    glfwMakeContextCurrent(nullptr);
}

int
main() {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;

    int         width = 256, height = 256;
    GLFWwindow *window = glfwCreateWindow(width, height, "GLFW Window", nullptr, nullptr);
    if (window == nullptr) {
        exit(EXIT_FAILURE);
    }
    glfwSetFramebufferSizeCallback(window, glfw_framebuffer_size_callback);
    auto render_thread = std::thread(render_loop, window, width, height, 120.0f);

    while (!glfwWindowShouldClose(window)) {
        glfwWaitEvents();
    }

    render_thread.join();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
