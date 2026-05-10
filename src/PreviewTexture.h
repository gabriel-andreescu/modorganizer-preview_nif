#pragma once

#include "OpenGLResources.h"

class PreviewTexture {
public:
    explicit PreviewTexture(QOpenGLTexture* texture);
    PreviewTexture(GLuint textureId, GLenum target);
    ~PreviewTexture();
    PreviewTexture(const PreviewTexture&) = delete;
    PreviewTexture(PreviewTexture&&) = delete;
    PreviewTexture& operator=(const PreviewTexture&) = delete;
    PreviewTexture& operator=(PreviewTexture&&) = delete;

    void bind(int textureUnit) const;
    void destroyWithCurrentContext();

private:
    QtOpenGLTextureResource m_QtTexture;
    OpenGLTextureResource m_RawTexture;
};
