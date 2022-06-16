//
// Created by taylor-santos on 5/16/2021 at 14:58.
//

#include "transform.hpp"
#include "glm/ext/matrix_relational.hpp"

#include <stdexcept>

#define EPSILON 0.00001

/***
 * Convert a rotation matrix to its equivalent quaternion.
 * @param mat a 3x3 rotation matrix
 * @return the quaternion representing the same rotation as the input matrix
 */
static glm::dquat
matToQuat(glm::dmat3 mat) {
    // https://www.euclideanspace.com/maths/geometry/rotations/conversions/matrixToQuaternion/
    glm::dquat q;
    auto       trace = mat[0][0] + mat[1][1] + mat[2][2];
    if (trace > 0) {
        auto s = 0.5 / glm::sqrt(trace + 1.0);
        q.w    = 0.25 / s;
        q.x    = (mat[1][2] - mat[2][1]) * s;
        q.y    = (mat[2][0] - mat[0][2]) * s;
        q.z    = (mat[0][1] - mat[1][0]) * s;
    } else {
        if (mat[0][0] > mat[1][1] && mat[0][0] > mat[2][2]) {
            auto s = 2.0 * glm::sqrt(1.0 + mat[0][0] - mat[1][1] - mat[2][2]);
            q.w    = (mat[1][2] - mat[2][1]) / s;
            q.x    = 0.25 * s;
            q.y    = (mat[1][0] + mat[0][1]) / s;
            q.z    = (mat[2][0] + mat[0][2]) / s;
        } else if (mat[1][1] > mat[2][2]) {
            auto s = 2.0 * glm::sqrt(1.0 + mat[1][1] - mat[0][0] - mat[2][2]);
            q.w    = (mat[2][0] - mat[0][2]) / s;
            q.x    = (mat[1][0] + mat[0][1]) / s;
            q.y    = 0.25 * s;
            q.z    = (mat[2][1] + mat[1][2]) / s;
        } else {
            auto s = 2.0 * glm::sqrt(1.0 + mat[2][2] - mat[0][0] - mat[1][1]);
            q.w    = (mat[0][1] - mat[1][0]) / s;
            q.x    = (mat[2][0] + mat[0][2]) / s;
            q.y    = (mat[2][1] + mat[1][2]) / s;
            q.z    = 0.25 * s;
        }
    }
    q = glm::normalize(q);
    return q;
}

/***
 * Apply the Cholesky decomposition to a given matrix.
 * @param A the 3x3 matrix to be decomposed
 * @return the Cholesky decomposition of the input matrix
 */
static glm::dmat3
cholesky(glm::dmat3 A) {
    // https://rosettacode.org/wiki/Cholesky_decomposition#C.2B.2B
    glm::dmat3 result{0};
    for (int i = 0; i < 3; ++i) {
        for (int k = 0; k < i; ++k) {
            auto value = A[i][k];
            for (int j = 0; j < k; ++j) value -= result[i][j] * result[k][j];
            result[i][k] = value / result[k][k];
        }
        auto value = A[i][i];
        for (int j = 0; j < i; ++j) value -= result[i][j] * result[i][j];
        result[i][i] = glm::sqrt(value);
    }
    return result;
}

Transform::Properties
Transform::decompose(glm::dmat4 mat) {
    // https://github.com/matthew-brett/transforms3d/blob/master/transforms3d/affines.py
    glm::dvec3 translation;
    glm::dquat rotation;
    glm::dvec3 scale;
    glm::dvec3 skew;
    translation    = mat[3];
    glm::dmat3 RZS = mat; // Strip off translation components
    auto       ZS  = cholesky(glm::transpose(RZS) * RZS);
    scale          = glm::dvec3{ZS[0][0], ZS[1][1], ZS[2][2]};
    auto ZST       = glm::transpose(ZS);
    auto shears =
        glm::transpose(glm::dmat3{ZST[0] / scale[0], ZST[1] / scale[1], ZST[2] / scale[2]});
    skew        = glm::dvec3{shears[1][0], shears[2][0], shears[2][1]};
    auto rotMat = RZS * glm::inverse(ZS);
    if (glm::determinant(rotMat) < 0) {
        scale[0] *= -1;
        ZS[0] *= -1;
        rotMat = RZS * glm::inverse(ZS);
    }
    rotation = matToQuat(rotMat);
    return {translation, rotation, scale, skew};
}

glm::dmat4
Transform::recompose(const Transform::Properties &mat) {
    auto       rotMat = glm::toMat3(mat.rotation);
    glm::dmat3 skewMat{1};
    skewMat[1][0] = mat.skew.x;
    skewMat[2][0] = mat.skew.y;
    skewMat[2][1] = mat.skew.z;
    glm::dmat3 scaleMat{0};
    scaleMat[0][0] = mat.scale[0];
    scaleMat[1][1] = mat.scale[1];
    scaleMat[2][2] = mat.scale[2];
    auto A         = rotMat * scaleMat * skewMat;
    return glm::dmat4{
        glm::dvec4{A[0], 0},
        glm::dvec4{A[1], 0},
        glm::dvec4{A[2], 0},
        glm::dvec4{mat.translation, 1}};
}

glm::dmat4
Transform::recomposeInverse(const Transform::Properties &mat) {
    auto       rotMat = glm::toMat4(glm::conjugate(mat.rotation));
    glm::dmat4 skewMat{1};
    skewMat[1][0] = -mat.skew.x;
    skewMat[2][0] = mat.skew.x * mat.skew.z - mat.skew.y;
    skewMat[2][1] = -mat.skew.z;
    glm::dmat4 scaleMat{1};
    scaleMat[0][0] = 1 / mat.scale[0];
    scaleMat[1][1] = 1 / mat.scale[1];
    scaleMat[2][2] = 1 / mat.scale[2];
    glm::dmat4 trMat{1};
    trMat[3] = glm::dvec4{-mat.translation, 1};
    return skewMat * scaleMat * rotMat * trMat;
}

Transform::Transform() noexcept = default;

Transform::Transform(Transform *parent, Properties properties)
    : parent_{parent}
    , locals_{properties} {
    if (parent_) {
        parent_->addChild(this);
    }
}

Transform::Transform(const Transform &other)
    : Transform(other.parent_, other.locals_) {}

Transform::Transform(Transform &&other) noexcept
    : Transform() {
    swap(*this, other);
}

Transform &
Transform::operator=(const Transform &other) {
    if (this == &other) return *this;
    Transform temp(other);
    swap(*this, temp);
    return *this;
}

Transform &
Transform::operator=(Transform &&other) noexcept {
    swap(*this, other);
    return *this;
}

Transform::~Transform() {
    if (parent_) {
        parent_->children_.erase(parentIt_);
    }
    while (!children_.empty()) {
        // Readjust children's local properties to keep them fixed in world space.
        auto child = children_.front();
        child->setParent(nullptr);
    }
}

bool
Transform::operator==(const Transform &other) const {
    if (parent_ != other.parent_) return false;
    if (glm::any(glm::epsilonNotEqual(locals_.translation, other.locals_.translation, EPSILON)))
        return false;
    if (glm::any(glm::notEqual(
            glm::toMat3(locals_.rotation),
            glm::toMat3(other.locals_.rotation),
            EPSILON)))
        return false;
    if (glm::any(glm::epsilonNotEqual(locals_.scale, other.locals_.scale, EPSILON))) return false;
    if (glm::any(glm::epsilonNotEqual(locals_.skew, other.locals_.skew, EPSILON))) return false;
    return true;
}

void
swap(Transform &first, Transform &second) noexcept {
    using std::swap;
    swap(first.parent_, second.parent_);
    swap(first.parentIt_, second.parentIt_);
    swap(first.children_, second.children_);
    swap(first.locals_, second.locals_);
    swap(first.cachedLocalToWorld_, second.cachedLocalToWorld_);
    swap(first.cachedWorldToLocal_, second.cachedWorldToLocal_);
    swap(first.cachedWorldProps_, second.cachedWorldProps_);
    for (auto child : first.children_) {
        child->parent_ = &first;
    }
    for (auto child : second.children_) {
        child->parent_ = &second;
    }
    if (first.parent_) {
        *first.parentIt_ = &first;
    }
    if (second.parent_) {
        *second.parentIt_ = &second;
    }
}

void
Transform::invalidateCache() const {
    cachedLocalToWorld_.reset();
    cachedWorldToLocal_.reset();
    cachedWorldProps_.reset();
    for (auto child : children_) {
        // Don't need to invalidate children's cache if they've already been invalidated
        if (child->cachedLocalToWorld_ || child->cachedWorldToLocal_ || child->cachedWorldProps_) {
            child->invalidateCache();
        }
    }
}

void
Transform::addChild(Transform *child) {
    children_.push_back(child);
    child->parentIt_ = --children_.end();
}

Transform &
Transform::setParent(Transform *parent, bool preserveLocalSpace) {
    if (parent == parent_) return *this;
    // Check if parent will create a cycle
    for (auto curr = parent; curr; curr = curr->parent_) {
        if (curr == this) {
            throw std::invalid_argument("Setting transform's parent would create a cycle");
        }
    }
    if (!preserveLocalSpace) {
        // Get the current world transformation
        auto mat = localToWorldMatrix();
        if (parent) {
            // Remove the new parent's transformation from the matrix;
            mat = parent->worldToLocalMatrix() * mat;
        }
        locals_ = decompose(mat);
    }
    if (parent_) {
        parent_->children_.erase(parentIt_);
    }
    parent_ = parent;
    if (parent_) {
        parent_->addChild(this);
    }
    invalidateCache();
    return *this;
}

Transform &
Transform::setPosition(glm::dvec3 position) {
    auto props         = decompose(localToWorldMatrix());
    props.translation  = position;
    auto localToWorld  = recompose(props);
    auto localToParent = localToWorld;
    if (parent_) {
        localToParent = parent_->worldToLocalMatrix() * localToParent;
    }
    locals_.translation = decompose(localToParent).translation;
    invalidateCache();
    cachedLocalToWorld_ = localToWorld;
    cachedWorldProps_   = props;
    return *this;
}

Transform &
Transform::setLocalPosition(glm::dvec3 localPosition) {
    if (localPosition == locals_.translation) {
        return *this;
    }
    locals_.translation = localPosition;
    invalidateCache();
    return *this;
}

Transform &
Transform::setRotation(glm::dquat rotation) {
    auto props         = decompose(localToWorldMatrix());
    props.rotation     = rotation;
    auto localToWorld  = recompose(props);
    auto localToParent = localToWorld;
    if (parent_) {
        localToParent = parent_->worldToLocalMatrix() * localToParent;
    }
    locals_.rotation = decompose(localToParent).rotation;
    invalidateCache();
    cachedLocalToWorld_ = localToWorld;
    cachedWorldProps_   = props;
    return *this;
}

Transform &
Transform::setLocalRotation(glm::dquat localRotation) {
    if (localRotation == locals_.rotation) {
        return *this;
    }
    locals_.rotation = glm::normalize(localRotation);
    invalidateCache();
    return *this;
}

Transform &
Transform::setScale(glm::dvec3 scale) {
    auto props         = decompose(localToWorldMatrix());
    props.scale        = scale;
    auto localToWorld  = recompose(props);
    auto localToParent = localToWorld;
    if (parent_) {
        localToParent = parent_->worldToLocalMatrix() * localToParent;
    }
    locals_.scale = decompose(localToParent).scale;
    invalidateCache();
    cachedLocalToWorld_ = localToWorld;
    cachedWorldProps_   = props;
    return *this;
}

Transform &
Transform::setLocalScale(glm::dvec3 localScale) {
    if (localScale == locals_.scale) {
        return *this;
    }
    locals_.scale = localScale;
    invalidateCache();
    return *this;
}

Transform &
Transform::setSkew(glm::dvec3 skew) {
    auto props         = decompose(localToWorldMatrix());
    props.skew         = skew;
    auto localToWorld  = recompose(props);
    auto localToParent = localToWorld;
    if (parent_) {
        localToParent = parent_->worldToLocalMatrix() * localToParent;
    }
    locals_.skew = decompose(localToParent).skew;
    invalidateCache();
    cachedLocalToWorld_ = localToWorld;
    cachedWorldProps_   = props;
    return *this;
}

Transform &
Transform::setLocalSkew(glm::dvec3 localSkew) {
    if (localSkew == locals_.skew) {
        return *this;
    }
    locals_.skew = localSkew;
    invalidateCache();
    return *this;
}

Transform *
Transform::parent() const {
    return parent_;
}

glm::dvec3
Transform::position() const {
    if (cachedWorldProps_) {
        return cachedWorldProps_->translation;
    }
    auto props        = decompose(localToWorldMatrix());
    cachedWorldProps_ = props;
    return props.translation;
}

glm::dvec3
Transform::localPosition() const {
    return locals_.translation;
}

glm::dquat
Transform::rotation() const {
    if (cachedWorldProps_) {
        return cachedWorldProps_->rotation;
    }
    auto props        = decompose(localToWorldMatrix());
    cachedWorldProps_ = props;
    return props.rotation;
}

glm::dquat
Transform::localRotation() const {
    return locals_.rotation;
}

glm::dvec3
Transform::scale() const {
    if (cachedWorldProps_) {
        return cachedWorldProps_->scale;
    }
    auto props        = decompose(localToWorldMatrix());
    cachedWorldProps_ = props;
    return props.scale;
}

glm::dvec3
Transform::localScale() const {
    return locals_.scale;
}

glm::dvec3
Transform::skew() const {
    if (cachedWorldProps_) {
        return cachedWorldProps_->skew;
    }
    auto props        = decompose(localToWorldMatrix());
    cachedWorldProps_ = props;
    return props.skew;
}

[[nodiscard]] glm::dvec3
Transform::localSkew() const {
    return locals_.skew;
}

glm::dvec3
Transform::right() const {
    return rotation() * glm::dvec3{1, 0, 0};
}

glm::dvec3
Transform::up() const {
    return rotation() * glm::dvec3{0, 1, 0};
}

glm::dvec3
Transform::forward() const {
    return rotation() * glm::dvec3{0, 0, -1};
}

glm::dmat4
Transform::parentToLocalMatrix() const {
    return recomposeInverse(locals_);
}

glm::dmat4
Transform::localToParentMatrix() const {
    return recompose(locals_);
}

glm::dmat4
Transform::worldToLocalMatrix() const {
    if (cachedWorldToLocal_) {
        return cachedWorldToLocal_.value();
    }
    glm::dmat4 mat = parentToLocalMatrix();
    if (parent_) {
        mat = mat * parent_->worldToLocalMatrix();
    }
    cachedWorldToLocal_ = mat;
    return mat;
}
glm::dmat4
Transform::localToWorldMatrix() const {
    if (cachedLocalToWorld_) {
        return cachedLocalToWorld_.value();
    }
    glm::dmat4 mat = localToParentMatrix();
    if (parent_) {
        mat = parent_->localToWorldMatrix() * mat;
    }
    cachedLocalToWorld_ = mat;
    return mat;
}

Transform::Builder &
Transform::Builder::withParent(Transform &parent) {
    parent_ = &parent;
    return *this;
}

Transform::Builder &
Transform::Builder::withPosition(glm::dvec3 position) {
    position_ = position;
    return *this;
}

Transform::Builder &
Transform::Builder::withRotation(glm::dquat rotation) {
    rotation_ = rotation;
    return *this;
}

Transform::Builder &
Transform::Builder::withScale(glm::dvec3 scale) {
    scale_ = scale;
    return *this;
}

Transform::Builder &
Transform::Builder::withSkew(glm::dvec3 skew) {
    skew_ = skew;
    return *this;
}

Transform
Transform::Builder::build() const {
    return Transform(parent_, Transform::Properties{position_, rotation_, scale_, skew_});
}

Transform::Builder::operator Transform() const {
    return build();
}
