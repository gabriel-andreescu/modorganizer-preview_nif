#pragma once

#include "Render/Shape.h"

#include <QMatrix4x4>

#include <vector>

// Alpha-pass shapes from farthest to nearest. Blended shapes can write depth, so a nearer one drawn first hides
// the ones behind it.
[[nodiscard]] std::vector<const OpenGLShape*> BackToFrontAlphaShapes(
    const std::vector<OpenGLShape>& a_shapes,
    const QMatrix4x4& a_viewMatrix
);
