#define CL_HPP_ENABLE_EXCEPTIONS
#define CL_HPP_TARGET_OPENCL_VERSION 200

#define NOMINMAX
#include <GL/gl3w.h>
#include <CL/opencl.hpp>
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#ifdef WIN32
#elif __APPLE__
#else
#    include <GL/glx.h>
#endif

#include <iostream>
#include <vector>
#include <algorithm>

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

    GLuint program = glCreateProgram();
    {
        const char *src    = "#version 330\n"
                             "layout(location = 0) in vec4 vposition;\n"
                             "layout(location = 1) in vec2 vtexcoord;\n"
                             "out vec2 ftexcoord;\n"
                             "void main() {\n"
                             "    ftexcoord = vtexcoord;\n"
                             "    gl_Position = vposition;\n"
                             "}\n";
        GLuint      shader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);
        glAttachShader(program, shader);
    }
    {
        const char *src    = "#version 330\n"
                             "uniform sampler2D tex;\n"
                             "in vec2 ftexcoord;\n"
                             "layout(location = 0) out vec4 fcolor;\n"
                             "void main() {\n"
                             "    fcolor = texture(tex, ftexcoord);\n"
                             "}\n";
        GLuint      shader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);
        glAttachShader(program, shader);
    }
    glLinkProgram(program);
    glUseProgram(program);
    GLint tex_loc = glGetUniformLocation(program, "tex");

    GLfloat vertexData[] = {
        // {x,y,z,u,v}
        1.0f,  1.0f,  0.0f, 1.0f, 1.0f, // vertex 0
        -1.0f, 1.0f,  0.0f, 0.0f, 1.0f, // vertex 1
        1.0f,  -1.0f, 0.0f, 1.0f, 0.0f, // vertex 2
        -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, // vertex 3
    };
    GLuint indexData[] = {
        0,
        1,
        2, // triangle 0
        2,
        1,
        3 // triangle 1
    };

    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertexData), vertexData, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), nullptr);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(
        1,
        2,
        GL_FLOAT,
        GL_FALSE,
        5 * sizeof(GLfloat),
        (char *)(0 + 3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    GLuint ebo;
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indexData), indexData, GL_STATIC_DRAW);

    GLuint texture;
    glGenTextures(1, &texture);

    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(
        GL_TEXTURE_2D,    // target
        0,                // mipmap level
        GL_RGBA8,         // internal format
        width,            // width in pixels
        height,           // height in pixels
        0,                // border, must be 0
        GL_RGBA,          // format
        GL_UNSIGNED_BYTE, // type
        nullptr           // data
    );

    std::vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);
    auto platform = cl::Platform::getDefault();
    auto device   = cl::Device::getDefault();

#ifdef WIN32
    cl_context_properties props[] = {
        CL_CONTEXT_PLATFORM,
        (cl_context_properties)platform(),
        CL_GL_CONTEXT_KHR,
        (cl_context_properties)wglGetCurrentContext(),
        CL_WGL_HDC_KHR,
        (cl_context_properties)wglGetCurrentDC(),
        0};
#elif __APPLE__
    CGLContextObj    glContext  = CGLGetCurrentContext();
    CGLShareGroupObj shareGroup = CGLGetShareGroup(glContext);

    cl_context_properties props[] = {
        CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE,
        (cl_context_properties)shareGroup,
        0};
#else
    cl_context_properties props[] = {
        CL_CONTEXT_PLATFORM,
        (cl_context_properties)platform(),
        CL_GL_CONTEXT_KHR,
        (cl_context_properties)glXGetCurrentContext(),
        CL_GLX_DISPLAY_KHR,
        (cl_context_properties)glXGetCurrentDisplay(),
        0};
#endif

    auto        context     = cl::Context(device, props);
    std::string kernel_code = R"(
void kernel my_kernel(__write_only image2d_t output, __read_only int frame){
    int2 coord = (int2)(get_global_id(0), get_global_id(1));
    int i = coord.x + coord.y * get_image_width(output);
    float3 color = (float3) {
        ((coord.x >> 4) & 0xFF) / 255.0,
        (((coord.x & 0xF) << 4) + ((coord.y >> 8) & 0xF) ) / 255.0,
        (coord.y & 0xFF) / 255.0,
    };
    // float3 color = (float3){
    //     (i / 256 / 256) % 256 / 255.0,
    //     (i / 256) % 256 / 255.0,
    //     i % 256 / 255.0,
    // };
    // float3 color = (float3){
    //     i % 2 / 1.0,
    //     i / 2 % 2 / 1.0,
    //     i / 4 % 2 / 1.0,
    // };
    write_imagef(output, coord, (float4)(color, 1.0));
})";
    auto        sources     = cl::Program::Sources{kernel_code};
    auto        cl_program  = cl::Program(context, sources);
    auto        queue       = cl::CommandQueue(context, device);

    try {
        cl_program.build(device);
    } catch (cl::BuildError &e) {
        std::cout << "Error building:\n";
        for (auto &line : e.getBuildLog()) {
            std::cout << "\t" << line.first.getInfo<CL_DEVICE_NAME>() << ": " << line.second
                      << "\n";
        }

        exit(EXIT_FAILURE);
    }

    cl::ImageGL cl_texture;
    try {
        cl_texture = cl::ImageGL(context, CL_MEM_READ_WRITE, GL_TEXTURE_2D, 0, texture, nullptr);
    } catch (cl::Error &e) {
        // TODO: more than just an error code
        // force this exception by removing `props` from the context constructor
        std::cout << e.what() << "\n";
        exit(EXIT_FAILURE);
    }

    auto kernel = cl::Kernel(cl_program, "my_kernel");
    kernel.setArg(0, cl_texture);
    auto img   = std::vector<cl::Memory>{cl_texture};
    int  frame = 0;

    auto last_time  = glfwGetTime();
    int  last_frame = 0;
    while (!glfwWindowShouldClose(window)) {
        auto frame_start = glfwGetTime();
        frame++;
        if (new_window_dim.load()) {
            auto new_dim = new_window_dim.exchange(std::nullopt).value();

            width  = new_dim.width;
            height = new_dim.height;

            glViewport(0, 0, width, height);
            glDeleteTextures(1, &texture);
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(
                GL_TEXTURE_2D,    // target
                0,                // mipmap level
                GL_RGBA8,         // internal format
                width,            // width in pixels
                height,           // height in pixels
                0,                // border, must be 0
                GL_RGBA,          // format
                GL_UNSIGNED_BYTE, // type
                nullptr           // data
            );
            cl_texture =
                cl::ImageGL(context, CL_MEM_READ_WRITE, GL_TEXTURE_2D, 0, texture, nullptr);
            img = std::vector<cl::Memory>{cl_texture};
            kernel.setArg(0, cl_texture);
        }

        // Finish all GL commands before CL tries to acquire the texture
        glFinish();
        // Run OpenCL to render to texture
        kernel.setArg(1, frame);
        queue.enqueueAcquireGLObjects(&img);
        queue.enqueueNDRangeKernel(kernel, cl::NullRange, cl::NDRange(width, height));
        queue.enqueueReleaseGLObjects(&img);
        queue.finish();

        // Draw texture to the screen
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(program);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glUniform1i(tex_loc, 0);
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

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
