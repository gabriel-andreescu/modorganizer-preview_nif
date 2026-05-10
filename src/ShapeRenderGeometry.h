#pragma once

#include <Geometry.hpp>

#include <vector>

namespace nifly {
class NifFile;
class NiShape;
}

struct ShapeRenderGeometry {
    const std::vector<nifly::Vector3>* rawPositions = nullptr;
    const std::vector<nifly::Vector3>* rawNormals = nullptr;
    const std::vector<nifly::Vector3>* rawTangents = nullptr;
    const std::vector<nifly::Vector3>* rawBitangents = nullptr;

    std::vector<nifly::Vector3> skinnedPositions;
    std::vector<nifly::Vector3> skinnedNormals;
    std::vector<nifly::Vector3> skinnedTangents;
    std::vector<nifly::Vector3> skinnedBitangents;
    std::vector<nifly::Triangle> triangles;

    nifly::MatTransform modelTransform;
    nifly::BoundingSphere bounds;
    bool skinned = false;

    [[nodiscard]] const std::vector<nifly::Vector3>* positions() const;
    [[nodiscard]] const std::vector<nifly::Vector3>* normals() const;
    [[nodiscard]] const std::vector<nifly::Vector3>* tangents() const;
    [[nodiscard]] const std::vector<nifly::Vector3>* bitangents() const;
};

void validateShapeGeometry(nifly::NiShape* shape);
[[nodiscard]] ShapeRenderGeometry prepareShapeRenderGeometry(nifly::NifFile* nifFile, nifly::NiShape* shape);
