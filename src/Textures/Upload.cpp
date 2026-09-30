#include "Textures/Upload.h"
#include "Render/Resources.h"
#include "Textures/PreviewTexture.h"

#include <gli/gli.hpp>

#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLFunctions_2_1>
#include <QOpenGLTexture>
#include <QOpenGLVersionFunctionsFactory>
#include <QtGui/qopenglext.h>

#include <limits>

namespace {

bool HasUploadableExtents(const gli::texture& a_texture) {
    if (a_texture.levels() == 0 || a_texture.layers() != 1 || a_texture.faces() == 0) {
        return false;
    }

    if (a_texture.target() == gli::TARGET_2D && a_texture.faces() != 1) {
        return false;
    }
    if (a_texture.target() == gli::TARGET_CUBE && a_texture.faces() != 6) {
        return false;
    }

    for (std::size_t level = 0; level < a_texture.levels(); ++level) {
        const auto extent = a_texture.extent(level);
        if (extent.x <= 0
            || extent.y <= 0
            || extent.z <= 0
            || extent.x > std::numeric_limits<GLsizei>::max()
            || extent.y > std::numeric_limits<GLsizei>::max()
            || a_texture.size(level) > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max())) {
            return false;
        }
    }

    return a_texture.levels() <= static_cast<std::size_t>(std::numeric_limits<GLsizei>::max());
}

PFNGLTEXSTORAGE2DPROC ResolveTexStorage2D(const QOpenGLContext* a_context) {
    return a_context ? reinterpret_cast<PFNGLTEXSTORAGE2DPROC>(a_context->getProcAddress("glTexStorage2D")) : nullptr;
}

void ClearGlErrors(QOpenGLFunctions_2_1* a_f) {
    while (a_f->glGetError() != GL_NO_ERROR) {}
}

GLenum TextureFaceTarget(const gli::texture& a_texture, const GLenum a_target, const std::size_t a_face) {
    return gli::is_target_cube(a_texture.target()) ? static_cast<GLenum>(GL_TEXTURE_CUBE_MAP_POSITIVE_X + a_face)
                                                   : a_target;
}

GLenum TextureUploadTarget(const gli::target a_target) {
    if (a_target == gli::TARGET_2D) {
        return GL_TEXTURE_2D;
    }

    if (a_target == gli::TARGET_CUBE) {
        return GL_TEXTURE_CUBE_MAP;
    }

    return 0;
}

void SetTextureParameters(
    QOpenGLFunctions_2_1* a_f,
    const GLenum a_target,
    const gli::gl::format& a_format,
    const std::size_t a_levels
) {
    a_f->glTexParameteri(a_target, GL_TEXTURE_BASE_LEVEL, 0);
    a_f->glTexParameteri(a_target, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(a_levels - 1));
    a_f->glTexParameteri(a_target, GL_TEXTURE_MIN_FILTER, a_levels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    a_f->glTexParameteri(a_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    a_f->glTexParameteri(a_target, GL_TEXTURE_WRAP_S, GL_REPEAT);
    a_f->glTexParameteri(a_target, GL_TEXTURE_WRAP_T, GL_REPEAT);
    a_f->glTexParameteri(a_target, static_cast<GLenum>(QOpenGLTexture::SwizzleRed), a_format.Swizzles[0]);
    a_f->glTexParameteri(a_target, static_cast<GLenum>(QOpenGLTexture::SwizzleGreen), a_format.Swizzles[1]);
    a_f->glTexParameteri(a_target, static_cast<GLenum>(QOpenGLTexture::SwizzleBlue), a_format.Swizzles[2]);
    a_f->glTexParameteri(a_target, static_cast<GLenum>(QOpenGLTexture::SwizzleAlpha), a_format.Swizzles[3]);
}

struct TextureUploadJob {
    const gli::texture& texture;
    QOpenGLFunctions_2_1* functions;
    GLenum target;
    const gli::gl::format& format;
    bool useStorage;
};

void UploadTextureLevel(const TextureUploadJob& a_upload, const std::size_t a_face, const std::size_t a_level) {
    const auto& texture = a_upload.texture;
    const auto& format = a_upload.format;
    auto* const functions = a_upload.functions;
    const auto extent = texture.extent(a_level);
    const auto targetFace = TextureFaceTarget(texture, a_upload.target, a_face);
    const auto* textureData = texture.data(0, a_face, a_level);
    const auto textureSize = static_cast<GLsizei>(texture.size(a_level));
    const auto level = static_cast<GLint>(a_level);

    if (gli::is_compressed(texture.format())) {
        if (a_upload.useStorage) {
            functions->glCompressedTexSubImage2D(
                targetFace,
                level,
                0,
                0,
                extent.x,
                extent.y,
                format.Internal,
                textureSize,
                textureData
            );
        } else {
            functions->glCompressedTexImage2D(
                targetFace,
                level,
                format.Internal,
                extent.x,
                extent.y,
                0,
                textureSize,
                textureData
            );
        }
        return;
    }

    if (a_upload.useStorage) {
        functions
            ->glTexSubImage2D(targetFace, level, 0, 0, extent.x, extent.y, format.External, format.Type, textureData);
    } else {
        functions->glTexImage2D(
            targetFace,
            level,
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

GLenum UploadTextureData(const TextureUploadJob& a_upload, PFNGLTEXSTORAGE2DPROC a_glTexStorage2D) {
    const auto& texture = a_upload.texture;
    if (a_upload.useStorage) {
        const auto extent = texture.extent();
        a_glTexStorage2D(
            a_upload.target,
            static_cast<GLsizei>(texture.levels()),
            a_upload.format.Internal,
            extent.x,
            extent.y
        );
    }

    for (std::size_t face = 0; face < texture.faces(); ++face) {
        for (std::size_t level = 0; level < texture.levels(); ++level) {
            UploadTextureLevel(a_upload, face, level);
        }
    }

    return a_upload.functions->glGetError();
}

OpenGLTextureResource MakeRawTexture(
    const gli::texture& a_texture,
    QOpenGLFunctions_2_1* a_f,
    const GLenum a_target,
    const gli::gl::format& a_format,
    PFNGLTEXSTORAGE2DPROC a_glTexStorage2D
) {
    for (const bool useStorage : {true, false}) {
        if (useStorage && !a_glTexStorage2D) {
            continue;
        }

        GLuint textureId = 0;
        a_f->glGenTextures(1, &textureId);
        if (textureId == 0) {
            continue;
        }

        OpenGLTextureResource textureResource(textureId, a_target);
        a_f->glBindTexture(a_target, textureResource.Id());
        SetTextureParameters(a_f, a_target, a_format, a_texture.levels());
        ClearGlErrors(a_f);
        a_f->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        const auto error = UploadTextureData(
            {
                .texture = a_texture,
                .functions = a_f,
                .target = a_target,
                .format = a_format,
                .useStorage = useStorage,
            },
            a_glTexStorage2D
        );

        a_f->glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        a_f->glBindTexture(a_target, 0);

        if (error == GL_NO_ERROR) {
            return textureResource;
        }

        qWarning(
            "Failed to upload DDS texture with %s storage: OpenGL error 0x%x",
            useStorage ? "immutable" : "mutable",
            error
        );
        textureResource.DestroyWithCurrentContext(a_f);
    }

    return {};
}

} // namespace

std::unique_ptr<PreviewTexture> TextureUpload::Upload(const gli::texture& a_texture) {
    if (a_texture.empty()) {
        return nullptr;
    }

    if (!HasUploadableExtents(a_texture)) {
        qWarning("Skipping DDS texture with invalid or unsupported image layout");
        return nullptr;
    }

    auto* context = QOpenGLContext::currentContext();
    auto* functions = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_2_1>(context);
    if (!functions) {
        qWarning("Skipping DDS texture: OpenGL 2.1 functions unavailable");
        return nullptr;
    }

    const gli::gl translator(gli::gl::PROFILE_GL33);
    const auto format = translator.translate(a_texture.format(), a_texture.swizzles());
    const auto target = TextureUploadTarget(a_texture.target());
    if (target == 0) {
        qWarning("Skipping DDS texture with unsupported OpenGL texture target");
        return nullptr;
    }

    auto textureResource = MakeRawTexture(a_texture, functions, target, format, ResolveTexStorage2D(context));
    if (!textureResource) {
        qWarning("Skipping DDS texture after failed OpenGL upload");
        return nullptr;
    }

    return std::make_unique<PreviewTexture>(textureResource.Release(), target);
}

std::unique_ptr<PreviewTexture> TextureUpload::MakeSolidColor(const QVector4D a_color) {
    auto* glTexture = new QOpenGLTexture(QOpenGLTexture::Target2D);
    glTexture->create();
    glTexture->bind();

    glTexture->setSize(1, 1);
    glTexture->setFormat(QOpenGLTexture::RGBA32F);
    glTexture->allocateStorage(QOpenGLTexture::RGBA, QOpenGLTexture::Float32);

    glTexture->setData(QOpenGLTexture::RGBA, QOpenGLTexture::Float32, &a_color);

    glTexture->release();
    return std::make_unique<PreviewTexture>(glTexture);
}
