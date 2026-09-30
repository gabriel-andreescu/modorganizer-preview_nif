#pragma once

#include <nifly/Geometry.hpp>
#include <nifly/NifFile.hpp>

#include <cstddef>
#include <vector>

struct CollisionColor {
    float r = 1.0F;
    float g = 1.0F;
    float b = 1.0F;
    float a = 1.0F;
};

struct CollisionVertex {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
    float r = 1.0F;
    float g = 1.0F;
    float b = 1.0F;
    float a = 1.0F;
};

struct CollisionLineRange {
    std::size_t firstVertex = 0;
    std::size_t vertexCount = 0;
    CollisionColor color;
};

struct CollisionGeometry {
    std::vector<CollisionVertex> vertices;
    std::vector<CollisionLineRange> lineRanges;
    nifly::BoundingSphere bounds;

    [[nodiscard]] bool Empty() const {
        return vertices.empty();
    }
};

class CollisionGeometryBuilder {
public:
    [[nodiscard]] static CollisionGeometry Build(const nifly::NifFile* a_nifFile);
};
