#pragma once

#include "OpenGLResources.h"
#include "ShaderManager.h"

#include <Geometry.hpp>

#include <QMatrix4x4>

#include <array>

class QOpenGLFunctions_2_1;

namespace nifly {
class NifFile;
class NiShader;
class NiShape;
}

class OpenGLShapeGeometry {
public:
    void initialize(nifly::NifFile* nifFile, nifly::NiShape* niShape, nifly::NiShader* shader);
    void destroyWithCurrentContext();
    void draw(QOpenGLFunctions_2_1* f) const;
    void setupVertexAttributes(QOpenGLFunctions_2_1* f) const;

    [[nodiscard]] const QMatrix4x4& modelMatrix() const noexcept {
        return m_ModelMatrix;
    }
    [[nodiscard]] const nifly::BoundingSphere& bounds() const noexcept {
        return m_Bounds;
    }

private:
    static void setDefaultVertexAttributes(QOpenGLFunctions_2_1* f);
    void initializeColorBuffer(nifly::NifFile* nifFile, nifly::NiShape* niShape, nifly::NiShader* shader);

    OpenGLVertexArrayResource m_VertexArray;
    std::array<OpenGLBufferResource, ATTRIB_COUNT> m_VertexBuffers;
    OpenGLBufferResource m_IndexBuffer;
    GLsizei m_Elements = 0;
    QMatrix4x4 m_ModelMatrix;
    nifly::BoundingSphere m_Bounds;
};
