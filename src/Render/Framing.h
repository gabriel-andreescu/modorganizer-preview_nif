#pragma once

#include "Nif/BoundingBox.h"

#include <nifly/Object3d.hpp>

#include <QMatrix4x4>
#include <QVector3D>

#include <vector>

class OpenGLShape;

inline constexpr float kVerticalFieldOfView = 40.0F;

struct ClipPlanes {
    float nearPlane = 0.0F;
    float farPlane = 0.0F;
};

// NIF space is Z-up. The preview's view space is Y-up with X mirrored.
[[nodiscard]] QMatrix4x4 NifToViewSpace();
[[nodiscard]] QVector3D ToQVector(const nifly::Vector3& a_vector);
[[nodiscard]] QMatrix4x4 ToQMatrix(const nifly::MatTransform& a_transform);

// A sphere enclosing both spheres. An empty sphere, one with no radius, adds nothing.
[[nodiscard]] nifly::BoundingSphere Enclose(
    const nifly::BoundingSphere& a_first,
    const nifly::BoundingSphere& a_second
);

// Around the shapes where they are now.
[[nodiscard]] nifly::BoundingSphere SceneBounds(const std::vector<const OpenGLShape*>& a_shapes);
// Around the shapes as they were loaded, leaving out those that are hidden then.
[[nodiscard]] nifly::BoundingSphere RestSceneBounds(const std::vector<const OpenGLShape*>& a_shapes);
[[nodiscard]] BoundingBox SceneBox(const std::vector<OpenGLShape>& a_shapes);

// The camera distance at which a sphere fills the narrower of the two fields of view.
[[nodiscard]] float FitDistance(float a_radius, float a_aspectRatio);

// Planes enclosing a sphere whose center lies a_centerDepth in front of the camera.
[[nodiscard]] ClipPlanes FitClipPlanes(float a_centerDepth, float a_radius);
