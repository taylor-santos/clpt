//
// Created by taylor-santos on 6/9/2022 at 01:33.
//

#pragma once

namespace Render {

class Renderer {
public:
    explicit Renderer(const std::string &source, const char *kernel_name, int width, int height);

    void
    render() const;

    void
    resize(int width, int height);

    template<typename T>
    void
    setKernelArg(cl_uint index, const T &value) {
        cl_kernel_.setArg(index, value);
    }

private:
    class SharedTexture {
    public:
        SharedTexture(Renderer &cl, int width, int height);
        ~SharedTexture();

        void
        resize(int width, int height);

        [[nodiscard]] inline const std::vector<cl::Memory> &
        gl_objects() const {
            return gl_objects_;
        }

        [[nodiscard]] inline std::pair<int, int>
        dims() const {
            return {width_, height_};
        }

        [[nodiscard]] inline operator GLuint() const {
            return gl_texture_;
        }

    private:
        int                     width_;
        int                     height_;
        Renderer               &renderer_;
        GLuint                  gl_texture_;
        cl::ImageGL             cl_texture_;
        std::vector<cl::Memory> gl_objects_;
    };

    class GLProgram {
    public:
        GLProgram();
        ~GLProgram();

        [[nodiscard]] inline operator GLuint() const {
            return program_;
        }

    private:
        GLuint program_;
    };

    class GLvao {
    public:
        GLvao();
        ~GLvao();

        [[nodiscard]] inline operator GLuint() const {
            return vao_;
        }

    private:
        GLuint vao_ = 0;
        GLuint vbo_ = 0;
        GLuint ebo_ = 0;
    };

private:
    cl::Platform     cl_platform_;
    cl::Device       cl_device_;
    cl::Context      cl_context_;
    cl::CommandQueue cl_queue_;
    cl::Program      cl_program_;
    cl::Kernel       cl_kernel_;

    GLProgram gl_program_;
    GLvao     gl_vao_;
    GLint     gl_tex_loc_;

    SharedTexture texture_;
};

} // namespace Render
