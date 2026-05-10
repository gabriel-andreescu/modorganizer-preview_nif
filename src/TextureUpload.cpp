#include "TextureUpload.h"
#include "OpenGLResources.h"
#include "PreviewTexture.h"

#include <gli/gli.hpp>

#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLFunctions_2_1>
#include <QOpenGLTexture>
#include <QOpenGLVersionFunctionsFactory>
#include <QtGui/qopenglext.h>

#include <limits>

namespace {

bool hasUploadableExtents(const gli::texture& texture) {
    if (texture.levels() == 0 || texture.layers() != 1 || texture.faces() == 0) {
        return false;
    }

    if (texture.target() == gli::TARGET_2D && texture.faces() != 1) {
        return false;
    }
    if (texture.target() == gli::TARGET_CUBE && texture.faces() != 6) {
        return false;
    }

    for (std::size_t level = 0; level < texture.levels(); ++level) {
        const auto extent = texture.extent(level);
        if (extent.x
            <= 0
            || extent.y
            <= 0
            || extent.z
            <= 0
            || extent.x
            > std::numeric_limits<GLsizei>::max()
            || extent.y
            > std::numeric_limits<GLsizei>::max()
            || texture.size(level)
            > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max())) {
            return false;
        }
    }

    return texture.levels() <= static_cast<std::size_t>(std::numeric_limits<GLsizei>::max());
}

PFNGLTEXSTORAGE2DPROC resolveTexStorage2D(const QOpenGLContext* context) {
    return context ? reinterpret_cast<PFNGLTEXSTORAGE2DPROC>(context->getProcAddress("glTexStorage2D")) : nullptr;
}

void clearGlErrors(QOpenGLFunctions_2_1* f) {
    while (f->glGetError() != GL_NO_ERROR) {}
}

GLenum textureFaceTarget(const gli::texture& texture, const GLenum target, const std::size_t face) {
    return gli::is_target_cube(texture.target()) ? static_cast<GLenum>(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face) : target;
}

GLenum textureUploadTarget(const gli::target target) {
    if (target == gli::TARGET_2D) {
        return GL_TEXTURE_2D;
    }

    if (target == gli::TARGET_CUBE) {
        return GL_TEXTURE_CUBE_MAP;
    }

    return 0;
}

void setTextureParameters(
    QOpenGLFunctions_2_1* f,
    const GLenum target,
    const gli::gl::format& format,
    const std::size_t levels
) {
    f->glTexParameteri(target, GL_TEXTURE_BASE_LEVEL, 0);
    f->glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(levels - 1));
    f->glTexParameteri(target, GL_TEXTURE_MIN_FILTER, levels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    f->glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    f->glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_REPEAT);
    f->glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_REPEAT);
    f->glTexParameteri(target, static_cast<GLenum>(QOpenGLTexture::SwizzleRed), format.Swizzles[0]);
    f->glTexParameteri(target, static_cast<GLenum>(QOpenGLTexture::SwizzleGreen), format.Swizzles[1]);
    f->glTexParameteri(target, static_cast<GLenum>(QOpenGLTexture::SwizzleBlue), format.Swizzles[2]);
    f->glTexParameteri(target, static_cast<GLenum>(QOpenGLTexture::SwizzleAlpha), format.Swizzles[3]);
}

GLenum uploadTextureData(
    const gli::texture& texture,
    QOpenGLFunctions_2_1* f,
    PFNGLTEXSTORAGE2DPROC glTexStorage2D,
    const GLenum target,
    const gli::gl::format& format,
    const bool useStorage
) {
    if (useStorage) {
        const auto extent = texture.extent();
        glTexStorage2D(target, static_cast<GLsizei>(texture.levels()), format.Internal, extent.x, extent.y);
    }

    for (std::size_t face = 0; face < texture.faces(); ++face) {
        for (std::size_t level = 0; level < texture.levels(); ++level) {
            const auto extent = texture.extent(level);
            const auto targetFace = textureFaceTarget(texture, target, face);
            const auto* textureData = texture.data(0, face, level);
            const auto textureSize = static_cast<GLsizei>(texture.size(level));

            if (gli::is_compressed(texture.format())) {
                if (useStorage) {
                    f->glCompressedTexSubImage2D(
                        targetFace,
                        static_cast<GLint>(level),
                        0,
                        0,
                        extent.x,
                        extent.y,
                        format.Internal,
                        textureSize,
                        textureData
                    );
                } else {
                    f->glCompressedTexImage2D(
                        targetFace,
                        static_cast<GLint>(level),
                        format.Internal,
                        extent.x,
                        extent.y,
                        0,
                        textureSize,
                        textureData
                    );
                }
            } else if (useStorage) {
                f->glTexSubImage2D(
                    targetFace,
                    static_cast<GLint>(level),
                    0,
                    0,
                    extent.x,
                    extent.y,
                    format.External,
                    format.Type,
                    textureData
                );
            } else {
                f->glTexImage2D(
                    targetFace,
                    static_cast<GLint>(level),
                    format.Internal,
                    extent.x,
                    extent.y,
                    0,
                    format.External,
                    format.Type,
                    textureData
                );
            }
        }
    }

    return f->glGetError();
}

OpenGLTextureResource makeRawTexture(
    const gli::texture& texture,
    QOpenGLFunctions_2_1* f,
    const GLenum target,
    const gli::gl::format& format,
    PFNGLTEXSTORAGE2DPROC glTexStorage2D
) {
    for (const bool useStorage : {true, false}) {
        if (useStorage && !glTexStorage2D) {
            continue;
        }

        GLuint textureId = 0;
        f->glGenTextures(1, &textureId);
        if (textureId == 0) {
            continue;
        }

        OpenGLTextureResource textureResource(textureId, target);
        f->glBindTexture(target, textureResource.id());
        setTextureParameters(f, target, format, texture.levels());
        clearGlErrors(f);
        f->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        const auto error = uploadTextureData(texture, f, glTexStorage2D, target, format, useStorage);

        f->glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        f->glBindTexture(target, 0);

        if (error == GL_NO_ERROR) {
            return textureResource;
        }

        qWarning(
            "Failed to upload DDS texture with %s storage: OpenGL error 0x%x",
            useStorage ? "immutable" : "mutable",
            error
        );
        textureResource.destroyWithCurrentContext(f);
    }

    return {};
}

} // namespace

std::unique_ptr<PreviewTexture> TextureUpload::upload(const gli::texture& texture) {
    if (texture.empty()) {
        return nullptr;
    }

    if (!hasUploadableExtents(texture)) {
        qWarning("Skipping DDS texture with invalid or unsupported image layout");
        return nullptr;
    }

    auto* context = QOpenGLContext::currentContext();
    auto* f = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_2_1>(context);
    if (!f) {
        qWarning("Skipping DDS texture: OpenGL 2.1 functions unavailable");
        return nullptr;
    }

    const gli::gl gl(gli::gl::PROFILE_GL33);
    const auto format = gl.translate(texture.format(), texture.swizzles());
    const auto target = textureUploadTarget(texture.target());
    if (target == 0) {
        qWarning("Skipping DDS texture with unsupported OpenGL texture target");
        return nullptr;
    }

    auto textureResource = makeRawTexture(texture, f, target, format, resolveTexStorage2D(context));
    if (!textureResource) {
        qWarning("Skipping DDS texture after failed OpenGL upload");
        return nullptr;
    }

    return std::make_unique<PreviewTexture>(textureResource.release(), target);
}

std::unique_ptr<PreviewTexture> TextureUpload::makeSolidColor(const QVector4D color) {
    auto* glTexture = new QOpenGLTexture(QOpenGLTexture::Target2D);
    glTexture->create();
    glTexture->bind();

    glTexture->setSize(1, 1);
    glTexture->setFormat(QOpenGLTexture::RGBA32F);
    glTexture->allocateStorage(QOpenGLTexture::RGBA, QOpenGLTexture::Float32);

    glTexture->setData(QOpenGLTexture::RGBA, QOpenGLTexture::Float32, &color);

    glTexture->release();
    return std::make_unique<PreviewTexture>(glTexture);
}
