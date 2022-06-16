//
// Created by taylor-santos on 6/9/2022 at 01:33.
//

#pragma once

#include <string>
#include <memory>
#include <functional>

namespace cl {
class CommandQueue;
class Kernel;
} // namespace cl

namespace Render {

class Renderer {
public:
    explicit Renderer(const std::string &source, const char *kernel_name, int width, int height);
    ~Renderer();

    void
    render() const;

    void
    resize(int width, int height);

    void
    setKernelArg(unsigned int index, size_t size, const void *value);

    void
    addInputBuffer(unsigned int index, size_t size);

    void
    writeBuffer(unsigned int index, size_t size, void *ptr) const;

private:
    struct Impl;
    const std::unique_ptr<Impl, std::function<void(Impl *)>> impl_;

    int width_;
    int height_;
};

} // namespace Render