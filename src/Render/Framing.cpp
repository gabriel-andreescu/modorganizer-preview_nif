#include "Render/Framing.h"
#include "Render/Shape.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace {
// A 24-bit depth buffer still separates surfaces about 1/1700 of the far distance apart
// at the far plane while far is at most this multiple of near.
constexpr float kMaxDepthRatio = 1.0e4F;

nifly::BoundingSphere Enclose(const nifly::BoundingSphere& a_first, const nifly::BoundingSphere& a_second) {
    const auto offset = a_second.center - a_first.center;
    const auto distance = offset.length();
    if (distance + a_second.radius <= a_first.radius) {
        return a_first;
    }
    if (distance + a_first.radius <= a_second.radius) {
        return a_second;
    }
    const auto radius = (distance + a_first.radius + a_second.radius) / 2.0F;
    return {a_first.center + (offset * ((radius - a_first.radius) / distance)), radius};
}
}

QMatrix4x4 NifToViewSpace() {
    return {-1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1};
}

QVector3D ToQVector(const nifly::Vector3& a_vector) {
    return {a_vector.x, a_vector.y, a_vector.z};
}

nifly::BoundingSphere SceneBounds(const std::span<const OpenGLShape> a_shapes) {
    nifly::BoundingSphere bounds;
    for (const auto& shape : a_shapes) {
        if (shape.IsRefractionProxy() || shape.Bounds().radius <= 0.0F) {
            continue;
        }
        bounds = bounds.radius > 0.0F ? Enclose(bounds, shape.Bounds()) : shape.Bounds();
    }
    return bounds;
}

float FitDistance(const float a_radius, const float a_aspectRatio) {
    const auto halfVertical = kVerticalFieldOfView * std::numbers::pi_v<float> / 360.0F;
    const auto halfHorizontal = std::atan(std::tan(halfVertical) * a_aspectRatio);
    return a_radius / std::sin(std::min(halfVertical, halfHorizontal));
}

ClipPlanes FitClipPlanes(const float a_centerDepth, const float a_radius) {
    const auto farPlane = std::max(a_centerDepth, 0.0F) + a_radius;
    return {.nearPlane = std::max(a_centerDepth - a_radius, farPlane / kMaxDepthRatio), .farPlane = farPlane};
}
