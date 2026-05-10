#include "OpenGLResources.h"

#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLFunctions_2_1>
#include <QOpenGLVersionFunctionsFactory>

#include <utility>

namespace {
QOpenGLFunctions_2_1* currentOpenGLFunctions() {
    return QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_2_1>(QOpenGLContext::currentContext());
}
} // namespace

OpenGLBufferResource::~OpenGLBufferResource() {
    if (m_Buffer && m_Buffer->isCreated()) {
        qWarning("Leaking OpenGL buffer: destroyWithCurrentContext() was not called");
        [[maybe_unused]] auto* const leakedBuffer = m_Buffer.release();
    }
}

OpenGLBufferResource& OpenGLBufferResource::operator=(OpenGLBufferResource&& other) noexcept {
    if (this != &other) {
        if (m_Buffer && m_Buffer->isCreated()) {
            qWarning("Leaking OpenGL buffer: destroyWithCurrentContext() was not called before move assignment");
            [[maybe_unused]] auto* const leakedBuffer = m_Buffer.release();
        }
        m_Buffer = std::move(other.m_Buffer);
    }
    return *this;
}

QOpenGLBuffer* OpenGLBufferResource::create(const QOpenGLBuffer::Type type) {
    if (m_Buffer && m_Buffer->isCreated()) {
        qWarning("Replacing live OpenGL buffer without destroyWithCurrentContext()");
        [[maybe_unused]] auto* const leakedBuffer = m_Buffer.release();
    }

    m_Buffer = std::make_unique<QOpenGLBuffer>(type);
    return m_Buffer.get();
}

void OpenGLBufferResource::destroyWithCurrentContext() {
    if (!m_Buffer) {
        return;
    }

    if (m_Buffer->isCreated()) {
        m_Buffer->destroy();
    }
    m_Buffer.reset();
}

OpenGLVertexArrayResource::~OpenGLVertexArrayResource() {
    if (m_VertexArray && m_VertexArray->isCreated()) {
        qWarning("Leaking OpenGL vertex array: destroyWithCurrentContext() was not called");
        [[maybe_unused]] auto* const leakedVertexArray = m_VertexArray.release();
    }
}

OpenGLVertexArrayResource& OpenGLVertexArrayResource::operator=(OpenGLVertexArrayResource&& other) noexcept {
    if (this != &other) {
        if (m_VertexArray && m_VertexArray->isCreated()) {
            qWarning("Leaking OpenGL vertex array: destroyWithCurrentContext() was not called before move assignment");
            [[maybe_unused]] auto* const leakedVertexArray = m_VertexArray.release();
        }
        m_VertexArray = std::move(other.m_VertexArray);
    }
    return *this;
}

QOpenGLVertexArrayObject* OpenGLVertexArrayResource::create() {
    if (m_VertexArray && m_VertexArray->isCreated()) {
        qWarning("Replacing live OpenGL vertex array without destroyWithCurrentContext()");
        [[maybe_unused]] auto* const leakedVertexArray = m_VertexArray.release();
    }

    m_VertexArray = std::make_unique<QOpenGLVertexArrayObject>();
    return m_VertexArray.get();
}

void OpenGLVertexArrayResource::destroyWithCurrentContext() {
    if (!m_VertexArray) {
        return;
    }

    if (m_VertexArray->isCreated()) {
        m_VertexArray->destroy();
    }
    m_VertexArray.release()->deleteLater();
}

OpenGLTextureResource::OpenGLTextureResource(const GLuint textureId, const GLenum target)
    : m_TextureId(textureId)
    , m_Target(target) {}

OpenGLTextureResource::~OpenGLTextureResource() {
    if (m_TextureId != 0) {
        qWarning("Leaking OpenGL texture %u: destroyWithCurrentContext() was not called", m_TextureId);
    }
}

OpenGLTextureResource::OpenGLTextureResource(OpenGLTextureResource&& other) noexcept
    : m_TextureId(other.m_TextureId)
    , m_Target(other.m_Target) {
    other.m_TextureId = 0;
    other.m_Target = 0;
}

OpenGLTextureResource& OpenGLTextureResource::operator=(OpenGLTextureResource&& other) noexcept {
    if (this != &other) {
        if (m_TextureId != 0) {
            qWarning(
                "Leaking OpenGL texture %u: destroyWithCurrentContext() was not called before move assignment",
                m_TextureId
            );
        }
        m_TextureId = other.m_TextureId;
        m_Target = other.m_Target;
        other.m_TextureId = 0;
        other.m_Target = 0;
    }
    return *this;
}

void OpenGLTextureResource::set(const GLuint textureId, const GLenum target) {
    if (m_TextureId != 0) {
        qWarning("Replacing live OpenGL texture %u without destroyWithCurrentContext()", m_TextureId);
    }

    m_TextureId = textureId;
    m_Target = target;
}

GLuint OpenGLTextureResource::release() noexcept {
    const auto textureId = m_TextureId;
    m_TextureId = 0;
    m_Target = 0;
    return textureId;
}

void OpenGLTextureResource::destroyWithCurrentContext(QOpenGLFunctions_2_1* f) {
    if (m_TextureId == 0) {
        return;
    }

    if (!f) {
        f = currentOpenGLFunctions();
    }
    if (!f) {
        qWarning("Leaking OpenGL texture %u: OpenGL 2.1 functions unavailable", m_TextureId);
        return;
    }

    const auto textureId = m_TextureId;
    f->glDeleteTextures(1, &textureId);
    m_TextureId = 0;
    m_Target = 0;
}

void OpenGLTextureResource::bind(const int textureUnit) const {
    if (m_TextureId == 0) {
        return;
    }

    if (auto* const f = currentOpenGLFunctions()) {
        f->glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(textureUnit));
        bind(f);
    }
}

void OpenGLTextureResource::bind(QOpenGLFunctions_2_1* f) const {
    if (f && m_TextureId != 0) {
        f->glBindTexture(m_Target, m_TextureId);
    }
}

QtOpenGLTextureResource::QtOpenGLTextureResource(QOpenGLTexture* texture)
    : m_Texture(texture) {}

QtOpenGLTextureResource::~QtOpenGLTextureResource() {
    if (m_Texture && m_Texture->isCreated()) {
        qWarning("Leaking QOpenGLTexture: destroyWithCurrentContext() was not called");
        [[maybe_unused]] auto* const leakedTexture = m_Texture.release();
    }
}

QtOpenGLTextureResource& QtOpenGLTextureResource::operator=(QtOpenGLTextureResource&& other) noexcept {
    if (this != &other) {
        if (m_Texture && m_Texture->isCreated()) {
            qWarning("Leaking QOpenGLTexture: destroyWithCurrentContext() was not called before move assignment");
            [[maybe_unused]] auto* const leakedTexture = m_Texture.release();
        }
        m_Texture = std::move(other.m_Texture);
    }
    return *this;
}

void QtOpenGLTextureResource::destroyWithCurrentContext() {
    if (!m_Texture) {
        return;
    }

    if (m_Texture->isCreated()) {
        m_Texture->destroy();
    }
    m_Texture.reset();
}
