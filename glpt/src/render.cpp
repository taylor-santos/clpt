//
// Created by taylor-santos on 6/9/2022 at 01:35.
//

#include <string_view>
#include <iostream>

#define CL_HPP_ENABLE_EXCEPTIONS
#define CL_HPP_TARGET_OPENCL_VERSION 200

#define NOMINMAX
#include <GL/gl3w.h>
#include <CL/opencl.hpp>

#include "render.hpp"

#ifdef WIN32
#elif __APPLE__
#else
#    include <GL/glx.h>
#endif

static const char *
gl_error_string(GLenum err) {
    switch (err) {
        case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
        case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
        case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
        case GL_STACK_OVERFLOW: return "GL_STACK_OVERFLOW";
        case GL_STACK_UNDERFLOW: return "GL_STACK_UNDERFLOW";
        case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
        case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
        case GL_CONTEXT_LOST: return "GL_CONTEXT_LOST";
        default: return "Unknown error";
    }
}

static cl::Platform
get_cl_platform() {
    try {
        return cl::Platform::getDefault();
    } catch (...) {
        std::cerr << "Failed to initialize default CL Platform: ";
        throw;
    }
}

static cl::Device
get_cl_device() {
    try {
        return cl::Device::getDefault();
    } catch (...) {
        std::cerr << "Failed to initialize default CL Device: ";
        throw;
    }
}

static cl::Context
get_cl_context(const cl::Platform &platform, const cl::Device &device) {
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
    try {
        return {device, props};
    } catch (...) {
        std::cerr << "Failed to initialize CL Context: ";
        throw;
    }
}

cl::CommandQueue
get_cl_queue(const cl::Context &context, const cl::Device &device) {
    try {
        return {context, device};
    } catch (...) {
        std::cerr << "Failed to initialize CL Command Queue: ";
        throw;
    }
}

static cl::Program
get_cl_program(const cl::Context &context, const cl::Device &device, const std::string &source) {
    try {
        auto program = cl::Program{context, source};
        program.build(device);
        return program;
    } catch (...) {
        std::cerr << "Failed to build CL Program: ";
        throw;
    }
}

static cl::Kernel
get_cl_kernel(const cl::Program &program, const char *kernel_name) {
    try {
        return {program, kernel_name};
    } catch (...) {
        std::cerr << "Failed to initialize CL Kernel: ";
        throw;
    }
}

static GLuint
get_gl_texture(int width, int height) {
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

    return texture;
}

static cl::ImageGL
get_cl_image(const cl::Context &context, GLuint texture) {
    try {
        return {context, CL_MEM_READ_WRITE, GL_TEXTURE_2D, 0, texture, nullptr};
    } catch (...) {
        std::cerr << "Failed to create CL image from GL texture: ";
        throw;
    }
}

namespace Render {

Renderer::Renderer(const std::string &source, const char *kernel_name, int width, int height) try
    : cl_platform_{get_cl_platform()}
    , cl_device_{get_cl_device()}
    , cl_context_{get_cl_context(cl_platform_, cl_device_)}
    , cl_queue_{get_cl_queue(cl_context_, cl_device_)}
    , cl_program_{get_cl_program(cl_context_, cl_device_, source)}
    , cl_kernel_{get_cl_kernel(cl_program_, kernel_name)}
    , gl_program_()
    , gl_tex_loc_(glGetUniformLocation(gl_program_, "tex"))
    , texture_(*this, width, height) {
    // TODO: error check gl_tex_loc_

} catch (cl::BuildError &e) {
    for (auto &line : e.getBuildLog()) {
        std::cerr << line.first.getInfo<CL_DEVICE_NAME>() << ":\n" << line.second << "\n";
    }
    throw;
} catch (cl::Error &e) {
    std::cerr << "OpenCL Error " << e.err() << std::endl;
    throw;
}

void
Renderer::render() const {
    auto gl_objects      = texture_.gl_objects();
    auto [width, height] = texture_.dims();

    // Finish all GL commands before CL tries to acquire the texture
    glFinish();

    cl_queue_.enqueueAcquireGLObjects(&gl_objects);
    cl_queue_.enqueueNDRangeKernel(cl_kernel_, cl::NullRange, cl::NDRange(width, height));
    cl_queue_.enqueueReleaseGLObjects(&gl_objects);
    cl_queue_.finish();

    // Draw texture to the screen
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(gl_program_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glUniform1i(gl_tex_loc_, 0);
    glBindVertexArray(gl_vao_);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
}

void
Renderer::resize(int width, int height) {
    texture_.resize(width, height);
}

Renderer::SharedTexture::SharedTexture(Renderer &cl, int width, int height) try
    : width_{width}
    , height_{height}
    , renderer_{cl}
    , gl_texture_{get_gl_texture(width_, height_)}
    , cl_texture_{get_cl_image(renderer_.cl_context_, gl_texture_)}
    , gl_objects_{cl_texture_} {
    renderer_.setKernelArg(0, cl_texture_);
} catch (...) {
    std::cerr << "Failed to create GL Texture: ";
    throw;
}

Renderer::SharedTexture::~SharedTexture() {
    glDeleteTextures(1, &gl_texture_);
}

void
Renderer::SharedTexture::resize(int width, int height) {
    width_  = width;
    height_ = height;

    glViewport(0, 0, width_, height_);
    glDeleteTextures(1, &gl_texture_);

    gl_texture_ = get_gl_texture(width_, height_);
    cl_texture_ = get_cl_image(renderer_.cl_context_, gl_texture_);
    gl_objects_ = {cl_texture_};
    renderer_.setKernelArg(0, cl_texture_);
}

Renderer::GLProgram::GLProgram()
    : program_{glCreateProgram()} {
    if (!program_) {
        auto err = glGetError();
        std::cerr << "Error creating OpenGL Program: " << gl_error_string(err) << std::endl;
        throw std::runtime_error("OpenGL Error");
    }
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
        glAttachShader(program_, shader);
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
        glAttachShader(program_, shader);
    }
    glLinkProgram(program_);
    glUseProgram(program_);
}

Renderer::GLProgram::~GLProgram() {
    glDeleteProgram(program_);
}

Renderer::GLvao::GLvao() {
    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);

    GLfloat vertices[] = {
        // {x,y,z,u,v}
        1.0f,  1.0f,  0.0f, 1.0f, 1.0f, // vertex 0
        -1.0f, 1.0f,  0.0f, 0.0f, 1.0f, // vertex 1
        1.0f,  -1.0f, 0.0f, 1.0f, 0.0f, // vertex 2
        -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, // vertex 3
    };
    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Vertices
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), nullptr);
    glEnableVertexAttribArray(0);

    // UVs
    glVertexAttribPointer(
        1,
        2,
        GL_FLOAT,
        GL_FALSE,
        5 * sizeof(GLfloat),
        (char *)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    GLuint tris[] = {
        0,
        1,
        2, // triangle 0
        2,
        1,
        3 // triangle 1
    };
    glGenBuffers(1, &ebo_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(tris), tris, GL_STATIC_DRAW);
}

Renderer::GLvao::~GLvao() {
    glDeleteVertexArrays(1, &vao_);
    glDeleteBuffers(1, &vbo_);
    glDeleteBuffers(1, &ebo_);
}

} // namespace Render
