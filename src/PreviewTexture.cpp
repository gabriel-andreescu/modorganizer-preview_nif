#include "PreviewTexture.h"

PreviewTexture::PreviewTexture(QOpenGLTexture* texture)
    : m_QtTexture(texture) {}

PreviewTexture::PreviewTexture(const GLuint textureId, const GLenum target)
    : m_RawTexture(textureId, target) {}

PreviewTexture::~PreviewTexture() = default;

void PreviewTexture::bind(const int textureUnit) const {
    if (auto* const texture = m_QtTexture.get()) {
        texture->bind(textureUnit);
        return;
    }

    m_RawTexture.bind(textureUnit);
}

void PreviewTexture::destroyWithCurrentContext() {
    m_QtTexture.destroyWithCurrentContext();
    m_RawTexture.destroyWithCurrentContext();
}
