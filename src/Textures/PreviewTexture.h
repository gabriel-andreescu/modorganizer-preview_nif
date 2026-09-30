#pragma once

#include "Render/Resources.h"

class PreviewTexture {
public:
    explicit PreviewTexture(QOpenGLTexture* a_texture);
    PreviewTexture(GLuint a_textureId, GLenum a_target);
    ~PreviewTexture();
    PreviewTexture(const PreviewTexture&) = delete;
    PreviewTexture(PreviewTexture&&) = delete;
    PreviewTexture& operator=(const PreviewTexture&) = delete;
    PreviewTexture& operator=(PreviewTexture&&) = delete;

    void Bind(int a_textureUnit) const;
    void DestroyWithCurrentContext();

private:
    QtOpenGLTextureResource _qtTexture;
    OpenGLTextureResource _rawTexture;
};
