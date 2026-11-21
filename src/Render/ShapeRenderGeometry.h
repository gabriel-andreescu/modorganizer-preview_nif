#pragma once

#include "Nif/BoundingBox.h"
#include "Render/ShapeSkin.h"

#include <nifly/Geometry.hpp>

#include <vector>

namespace nifly {
class NifFile;
class NiShape;
}

struct ShapeRenderGeometry {
    SkinSource raw;
    // Filled for a skinned shape, whose vertices are drawn where its bones put them.
    SkinnedVertices deformed;
    std::vector<nifly::Triangle> triangles;

    nifly::MatTransform modelTransform;
    nifly::BoundingSphere bounds;
    // Around the vertices where they are drawn, after the model transform or skinning.
    BoundingBox box;
    bool skinned = false;

    [[nodiscard]] const std::vector<nifly::Vector3>* Positions() const;
    [[nodiscard]] const std::vector<nifly::Vector3>* Normals() const;
    [[nodiscard]] const std::vector<nifly::Vector3>* Tangents() const;
    [[nodiscard]] const std::vector<nifly::Vector3>* Bitangents() const;
};

void ValidateShapeGeometry(nifly::NiShape* a_shape);
[[nodiscard]] ShapeRenderGeometry PrepareShapeRenderGeometry(nifly::NifFile* a_nifFile, nifly::NiShape* a_shape);
