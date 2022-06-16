//
// Created by taylor-santos on 5/16/2021 at 14:58.
//

#pragma once

#define GLM_FORCE_SILENT_WARNINGS // Suppress 'nonstandard extension used: nameless struct/union'
#include "glm/glm.hpp"
#include "glm/gtx/quaternion.hpp"

#include <list>
#include <optional>

class Transform {
public:
    class Builder {
    public:
        Builder() = default;

        // Set the Transform's parent
        Builder &
        withParent(Transform &parent);

        // Set the Transform's position
        Builder &
        withPosition(glm::dvec3 position);

        // Set the Transform's rotation
        Builder &
        withRotation(glm::dquat rotation);

        // Set the Transform's scale
        Builder &
        withScale(glm::dvec3 scale);

        // Set the Transform's skew
        Builder &
        withSkew(glm::dvec3 skew);

        // Build the Transform with the requested properties
        [[nodiscard]] Transform
        build() const;

        // Implicit conversion operator - Use a Builder in a context where a Transform is expected,
        // and it will invoke build() automatically.
        operator Transform() const;

    private:
        Transform *parent_{nullptr};
        glm::dvec3 position_{0, 0, 0};
        glm::dquat rotation_{glm::quat_identity<double, glm::defaultp>()};
        glm::dvec3 scale_{1, 1, 1};
        glm::dvec3 skew_{0, 0, 0};
    };

    struct Properties {
        glm::dvec3 translation{0, 0, 0};
        glm::dquat rotation{glm::quat_identity<double, glm::defaultp>()};
        glm::dvec3 scale{1, 1, 1};
        glm::dvec3 skew{0, 0, 0};
    };

    /***
     * Decompose an affine matrix into its constituent parts: translation, rotation, scale, and
     * skew.
     * @param mat the 4x4 affine matrix to be decomposed
     * @return a struct that contains the translation, rotation, scale, and skew that make up the
     * matrix
     */
    static Properties
    decompose(glm::dmat4 mat);

    /***
     * Reconstruct an affine matrix from its translation, rotation, scale, and skew.
     * @param mat a struct containing translation, rotation, scale, and skew to be joined into a
     * matrix
     * @return a 4x4 affine matrix representing the same transformation as the input parameters
     */
    static glm::dmat4
    recompose(const Transform::Properties &mat);

    /***
     * Reconstruct an affine matrix from the inverse of its translation, rotation, scale, and skew.
     * @param mat a struct containing the inverse translation, rotation, scale, and skew to be
     * joined into a matrix
     * @return a 4x4 affine matrix representing the inverted transformation of the input parameters
     */
    static glm::dmat4
    recomposeInverse(const Transform::Properties &mat);

    Transform() noexcept;

    ~Transform();

    /**
     * Copy a Transform's physical characteristics and make the copy share the same parent. Does not
     * copy the original's children, if it has any.
     * @param other the Transform to be copied
     */
    Transform(const Transform &other);

    // Move constructor
    Transform(Transform &&other) noexcept;

    // Copy-assignment operator
    Transform &
    operator=(const Transform &other);

    // Move-assignment operator
    Transform &
    operator=(Transform &&other) noexcept;

    /**
     * Compare the physical characteristics of two Transforms. If either has a parent, they must
     * both share the same parent to be considered equal.
     * @param other the Transform to compare against
     * @return true if both Transforms have the same parent and characteristics, false otherwise
     */
    bool
    operator==(const Transform &other) const;

    // Swap two Transforms
    friend void
    swap(Transform &first, Transform &second) noexcept;

    /* Setters */

    /**
     * Set the Transform's parent, or remove its parent if nullptr is given instead.
     * If preserveLocalSpace is true, this Transform's local-space characteristics will be preserved
     * so that its local position, rotation, scale, and skew remain fixed relative to its new
     * parent.
     * If preserveLocalSpace is false, this Transform's world-space position, rotation, scale, and
     * skew will remain fixed.
     * Setting a Transform's parent must not create a cycle in the child->parent hierarchy. If a
     * cycle would be created with the given parent, an exception is thrown and the Transform will
     * remain unchanged.
     * @param parent a pointer to the new parent to be set, or nullptr to remove the current parent
     * @return a reference to this Transform, so setters may be chained
     * @throws std::invalid_argument if setting this parent would create a cycle in the Transform
     *         hierarchy
     */
    Transform &
    setParent(Transform *parent, bool preserveLocalSpace = false);

    // Set the Transform's world-space position.
    Transform &
    setPosition(glm::dvec3 position);

    // Set the Transform's local position relative to its parent.
    Transform &
    setLocalPosition(glm::dvec3 localPosition);

    // Set the Transform's world-space rotation.
    Transform &
    setRotation(glm::dquat rotation);

    // Set the Transform's local rotation relative to its parent.
    Transform &
    setLocalRotation(glm::dquat localRotation);

    // Set the Transform's world-space scale.
    Transform &
    setScale(glm::dvec3 scale);

    // Set the Transform's local scale relative to its parent.
    Transform &
    setLocalScale(glm::dvec3 localScale);

    // Set the Transform's world-space skew.
    Transform &
    setSkew(glm::dvec3 skew);

    // Set the Transform's local skew relative to its parent.
    Transform &
    setLocalSkew(glm::dvec3 localSkew);

    /* Getters */

    // Get this Transform's parent if it has one, or nullptr if not.
    [[nodiscard]] Transform *
    parent() const;

    // Get this Transform's world-space position.
    [[nodiscard]] glm::dvec3
    position() const;

    // Get this Transform's local-space position.
    [[nodiscard]] glm::dvec3
    localPosition() const;

    // Get this Transform's world-space rotation.
    [[nodiscard]] glm::dquat
    rotation() const;

    // Get this Transform's local-space rotation.
    [[nodiscard]] glm::dquat
    localRotation() const;

    // Get this Transform's world-space scale.
    [[nodiscard]] glm::dvec3
    scale() const;

    // Get this Transform's local-space scale.
    [[nodiscard]] glm::dvec3
    localScale() const;

    // Get this Transform's world-space skew.
    [[nodiscard]] glm::dvec3
    skew() const;

    // Get this Transform's local-space skew.
    [[nodiscard]] glm::dvec3
    localSkew() const;

    // Get the vector pointing right relative to this Transform's local-space. This is only based on
    // the Transform's rotation, not its position, scale or skew.
    [[nodiscard]] glm::dvec3
    right() const;

    // Get the vector pointing up relative to this Transform's local-space. This is only based on
    // the Transform's rotation, not its position, scale or skew.
    [[nodiscard]] glm::dvec3
    up() const;

    // Get the vector pointing forward relative to this Transform's local-space. This is only based
    // on the Transform's rotation, not its position, scale or skew. Note: because this uses a
    // right-handed coordinate system, this points in the negative direction along the z-axis
    [[nodiscard]] glm::dvec3
    forward() const;

    // Get the matrix transforming from this Transform's parent's local-space to this Transform's
    // local-space.
    [[nodiscard]] glm::dmat4
    parentToLocalMatrix() const;

    // Get the matrix transforming from this Transform's local-space to this Transform's parent's
    // local-space.
    [[nodiscard]] glm::dmat4
    localToParentMatrix() const;

    // Get the matrix transforming from world-space to this Transform's local-space.
    [[nodiscard]] glm::dmat4
    worldToLocalMatrix() const;

    // Get the matrix transforming from this Transform's local-space to world-space.
    [[nodiscard]] glm::dmat4
    localToWorldMatrix() const;

private:
    Transform                        *parent_{nullptr};
    std::list<Transform *>::iterator  parentIt_{};
    std::list<Transform *>            children_{};
    Properties                        locals_;
    mutable std::optional<glm::dmat4> cachedLocalToWorld_{};
    mutable std::optional<glm::dmat4> cachedWorldToLocal_{};
    mutable std::optional<Properties> cachedWorldProps_{};

private:
    Transform(Transform *parent, Properties properties);

    // Add a child to this Transform's children list, and store the resulting iterator in the
    // child's parentIt_ member.
    void
    addChild(Transform *child);

    // Clear all of this Transform's cached parameters, then invoke this method on all of its
    // children recursively.
    void
    invalidateCache() const;
};
