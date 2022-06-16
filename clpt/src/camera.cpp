//
// Created by taylor-santos on 5/10/2021 at 18:38.
//

#include "camera.hpp"

std::pair<float, float>
Camera::getSensitivity() const {
    return sens_;
}

void
Camera::setSensitivity(const std::pair<float, float> &sens) {
    sens_ = sens;
}

void
Camera::setSensitivity(float xSens, float ySens) {
    sens_ = {xSens, ySens};
}

float
Camera::getFOV() const {
    return glm::degrees(fov_);
}

void
Camera::setFOV(float fov) {
    fov_ = glm::radians(fov);
}

void
Camera::addRotation(float yaw, float pitch) {
    auto [xSens, ySens] = sens_;
    yaw_                = glm::radians(glm::mod(glm::degrees(yaw_) + yaw * xSens, 360.0f));
    pitch_ = glm::radians(glm::clamp(glm::degrees(pitch_) + pitch * ySens, -90.0f, 90.0f));
}

std::pair<float, float>
Camera::getRotation() const {
    return {glm::degrees(yaw_), glm::degrees(pitch_)};
}

void
Camera::setRotation(float yaw, float pitch) {
    yaw_   = glm::radians(glm::mod(yaw, 360.0f));
    pitch_ = glm::radians(glm::clamp(pitch, -90.0f, 90.0f));
}

float
Camera::getNear() const {
    return near_;
}

void
Camera::setNear(float near) {
    near_ = near;
}

float
Camera::getFar() const {
    return far_;
}

void
Camera::setFar(float far) {
    far_ = far;
}

glm::vec3
Camera::forward() const {
    float cosP = glm::cos(pitch_);
    float x    = -glm::sin(yaw_) * cosP;
    float y    = glm::sin(pitch_);
    float z    = glm::cos(yaw_) * cosP;
    auto  f    = glm::vec3(x, y, z);
    return f;
}

glm::vec3
Camera::right() const {
    return glm::vec3{-glm::cos(yaw_), 0, -glm::sin(yaw_)};
}

glm::vec3
Camera::up() const {
    return {
        glm::sin(pitch_) * glm::sin(yaw_),
        glm::cos(pitch_),
        -glm::cos(yaw_) * glm::sin(pitch_)};
}

static glm::vec3
cross_up(glm::vec3 v) {
    return {-v.z, 0, v.x};
}

glm::mat4
Camera::viewMatrix() const {
    glm::vec3 pos  = transform.position();
    glm::vec3 f    = forward();
    auto      view = glm::lookAt(pos, pos + forward(), up());

    auto c = pos + f;
    auto s = cross_up(f);
    auto u = glm::cross(s, f);
    auto M = glm::mat3x3{s, u, -f};
    auto x = M * -pos;

    return view;
}

glm::mat4
Camera::getMatrix(float aspect) const {
    if (std::isnan(aspect)) {
        aspect = 1;
    }

    glm::mat4 view       = viewMatrix();
    glm::mat4 projection = glm::perspective(fov_, aspect, near_, far_);
    glm::mat4 vp         = projection * view;
    return vp;
}
