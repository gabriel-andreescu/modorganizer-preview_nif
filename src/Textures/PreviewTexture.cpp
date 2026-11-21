#include "Textures/PreviewTexture.h"

#include <QOpenGLContext>
#include <QOpenGLVersionFunctionsFactory>

PreviewTexture::PreviewTexture(QOpenGLTexture* a_texture)
    : _qtTexture(a_texture) {}

PreviewTexture::PreviewTexture(const GLuint a_textureId, const GLenum a_target)
    : _rawTexture(a_textureId, a_target) {}

PreviewTexture::~PreviewTexture() = default;

void PreviewTexture::Bind(const int a_textureUnit, const TextureWrap& a_wrap) const {
    GLenum target = 0;
    if (auto* const texture = _qtTexture.Get()) {
        texture->bind(a_textureUnit);
        target = static_cast<GLenum>(texture->target());
    } else {
        _rawTexture.Bind(a_textureUnit);
        target = _rawTexture.Target();
    }

    // Shapes share cached textures and OpenGL 2.1 has no sampler objects, so every bind sets its shape's wrap mode.
    auto* const functions = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_2_1>(QOpenGLContext::currentContext());
    if (target == GL_TEXTURE_2D && functions) {
        functions->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, a_wrap.s);
        functions->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, a_wrap.t);
    }
}

void PreviewTexture::DestroyWithCurrentContext() {
    _qtTexture.DestroyWithCurrentContext();
    _rawTexture.DestroyWithCurrentContext();
}
