#pragma once

#include <Geometry.hpp>
#include <NifFile.hpp>

#include <cstddef>
#include <vector>

struct CollisionColor
{
  float r = 1.0f;
  float g = 1.0f;
  float b = 1.0f;
  float a = 1.0f;
};

struct CollisionVertex
{
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  float r = 1.0f;
  float g = 1.0f;
  float b = 1.0f;
  float a = 1.0f;
};

struct CollisionLineRange
{
  std::size_t firstVertex = 0;
  std::size_t vertexCount = 0;
  CollisionColor color;
};

struct CollisionGeometry
{
  std::vector<CollisionVertex> vertices;
  std::vector<CollisionLineRange> lineRanges;
  nifly::BoundingSphere bounds;

  [[nodiscard]] bool empty() const { return vertices.empty(); }
};

class CollisionGeometryBuilder
{
public:
  [[nodiscard]] static CollisionGeometry build(const nifly::NifFile* nifFile);
};
