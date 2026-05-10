#pragma once

#include "CollisionGeometry.h"

#include <QMatrix4x4>
#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>

#include <cstddef>
#include <vector>

class QOpenGLFunctions_2_1;

class OpenGLCollisionOverlay
{
public:
  explicit OpenGLCollisionOverlay(const CollisionGeometry& geometry);
  ~OpenGLCollisionOverlay() = default;

  OpenGLCollisionOverlay(const OpenGLCollisionOverlay&)            = delete;
  OpenGLCollisionOverlay(OpenGLCollisionOverlay&&)                 = delete;
  OpenGLCollisionOverlay& operator=(const OpenGLCollisionOverlay&) = delete;
  OpenGLCollisionOverlay& operator=(OpenGLCollisionOverlay&&)      = delete;

  void destroy();
  void render(QOpenGLShaderProgram* program, const QMatrix4x4& viewMatrix,
              const QMatrix4x4& projectionMatrix) const;

  [[nodiscard]] bool empty() const { return m_VertexCount == 0; }
  [[nodiscard]] const nifly::BoundingSphere& bounds() const { return m_Bounds; }

private:
  QOpenGLVertexArrayObject* m_VertexArray = nullptr;
  QOpenGLBuffer* m_VertexBuffer           = nullptr;
  std::vector<CollisionLineRange> m_LineRanges;
  std::size_t m_VertexCount = 0;
  nifly::BoundingSphere m_Bounds;
};
