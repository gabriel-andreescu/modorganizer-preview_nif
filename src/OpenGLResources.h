#pragma once

#include <QOpenGLBuffer>
#include <QOpenGLTexture>
#include <QOpenGLVertexArrayObject>

#include <memory>

class QOpenGLFunctions_2_1;

class OpenGLBufferResource final {
public:
    OpenGLBufferResource() = default;
    ~OpenGLBufferResource();
    OpenGLBufferResource(const OpenGLBufferResource&) = delete;
    OpenGLBufferResource(OpenGLBufferResource&&) noexcept = default;
    OpenGLBufferResource& operator=(const OpenGLBufferResource&) = delete;
    OpenGLBufferResource& operator=(OpenGLBufferResource&& other) noexcept;

    QOpenGLBuffer* create(QOpenGLBuffer::Type type);
    void destroyWithCurrentContext();

    [[nodiscard]] QOpenGLBuffer* get() const noexcept {
        return m_Buffer.get();
    }
    [[nodiscard]] explicit operator bool() const noexcept {
        return m_Buffer != nullptr;
    }
    [[nodiscard]] QOpenGLBuffer* operator->() const noexcept {
        return m_Buffer.get();
    }

private:
    std::unique_ptr<QOpenGLBuffer> m_Buffer;
};

class OpenGLVertexArrayResource final {
public:
    OpenGLVertexArrayResource() = default;
    ~OpenGLVertexArrayResource();
    OpenGLVertexArrayResource(const OpenGLVertexArrayResource&) = delete;
    OpenGLVertexArrayResource(OpenGLVertexArrayResource&&) noexcept = default;
    OpenGLVertexArrayResource& operator=(const OpenGLVertexArrayResource&) = delete;
    OpenGLVertexArrayResource& operator=(OpenGLVertexArrayResource&& other) noexcept;

    QOpenGLVertexArrayObject* create();
    void destroyWithCurrentContext();

    [[nodiscard]] QOpenGLVertexArrayObject* get() const noexcept {
        return m_VertexArray.get();
    }
    [[nodiscard]] explicit operator bool() const noexcept {
        return m_VertexArray != nullptr;
    }

private:
    std::unique_ptr<QOpenGLVertexArrayObject> m_VertexArray;
};

class OpenGLTextureResource final {
public:
    OpenGLTextureResource() = default;
    OpenGLTextureResource(GLuint textureId, GLenum target);
    ~OpenGLTextureResource();
    OpenGLTextureResource(const OpenGLTextureResource&) = delete;
    OpenGLTextureResource(OpenGLTextureResource&& other) noexcept;
    OpenGLTextureResource& operator=(const OpenGLTextureResource&) = delete;
    OpenGLTextureResource& operator=(OpenGLTextureResource&& other) noexcept;

    void set(GLuint textureId, GLenum target);
    [[nodiscard]] GLuint release() noexcept;
    void destroyWithCurrentContext(QOpenGLFunctions_2_1* f = nullptr);
    void bind(int textureUnit) const;
    void bind(QOpenGLFunctions_2_1* f) const;

    [[nodiscard]] bool isCreated() const noexcept {
        return m_TextureId != 0;
    }
    [[nodiscard]] GLuint id() const noexcept {
        return m_TextureId;
    }
    [[nodiscard]] GLenum target() const noexcept {
        return m_Target;
    }
    [[nodiscard]] explicit operator bool() const noexcept {
        return isCreated();
    }

private:
    GLuint m_TextureId = 0;
    GLenum m_Target = 0;
};

class QtOpenGLTextureResource final {
public:
    QtOpenGLTextureResource() = default;
    explicit QtOpenGLTextureResource(QOpenGLTexture* texture);
    ~QtOpenGLTextureResource();
    QtOpenGLTextureResource(const QtOpenGLTextureResource&) = delete;
    QtOpenGLTextureResource(QtOpenGLTextureResource&&) noexcept = default;
    QtOpenGLTextureResource& operator=(const QtOpenGLTextureResource&) = delete;
    QtOpenGLTextureResource& operator=(QtOpenGLTextureResource&& other) noexcept;

    void destroyWithCurrentContext();

    [[nodiscard]] QOpenGLTexture* get() const noexcept {
        return m_Texture.get();
    }
    [[nodiscard]] explicit operator bool() const noexcept {
        return m_Texture != nullptr;
    }
    [[nodiscard]] QOpenGLTexture* operator->() const noexcept {
        return m_Texture.get();
    }

private:
    std::unique_ptr<QOpenGLTexture> m_Texture;
};
