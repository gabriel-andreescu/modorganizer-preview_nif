#include "Textures/PreviewTexture.h"

PreviewTexture::PreviewTexture(QOpenGLTexture* a_texture)
    : _qtTexture(a_texture) {}

PreviewTexture::PreviewTexture(const GLuint a_textureId, const GLenum a_target)
    : _rawTexture(a_textureId, a_target) {}

PreviewTexture::~PreviewTexture() = default;

void PreviewTexture::Bind(const int a_textureUnit) const {
    if (auto* const texture = _qtTexture.Get()) {
        texture->bind(a_textureUnit);
        return;
    }

    _rawTexture.Bind(a_textureUnit);
}

void PreviewTexture::DestroyWithCurrentContext() {
    _qtTexture.DestroyWithCurrentContext();
    _rawTexture.DestroyWithCurrentContext();
}
