#include "Render/DrawOrder.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>

namespace {
// Layers sharing a plane, such as stained glass's multiply and additive passes, get bounds centers that differ by
// float rounding. Depths in 0.01-unit steps make them tie, so they keep the file's order.
constexpr float kDepthStep = 0.01F;

// OpenGL view space looks down -Z, so a smaller depth is farther away.
float ViewDepth(const OpenGLShape& a_shape, const QMatrix4x4& a_viewMatrix) {
    const auto& center = a_shape.Bounds().center;
    const float depth = (a_viewMatrix * a_shape.ModelMatrix()).map(QVector3D(center.x, center.y, center.z)).z();
    return std::round(depth / kDepthStep);
}
} // namespace

std::vector<const OpenGLShape*> BackToFrontAlphaShapes(
    const std::vector<OpenGLShape>& a_shapes,
    const QMatrix4x4& a_viewMatrix
) {
    std::vector<const OpenGLShape*> shapes;
    for (const auto& shape : a_shapes) {
        if (!shape.IsRefractionProxy() && shape.UsesAlphaPass()) {
            shapes.push_back(&shape);
        }
    }

    std::ranges::stable_sort(shapes, [&](const OpenGLShape* a_left, const OpenGLShape* a_right) {
        return ViewDepth(*a_left, a_viewMatrix) < ViewDepth(*a_right, a_viewMatrix);
    });
    return shapes;
}
