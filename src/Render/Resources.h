#pragma once

#include <QOpenGLBuffer>
#include <QOpenGLFunctions_2_1>
#include <QOpenGLTexture>
#include <QOpenGLVertexArrayObject>

#include <memory>

class OpenGLBufferResource final {
public:
    OpenGLBufferResource() = default;
    ~OpenGLBufferResource();
    OpenGLBufferResource(const OpenGLBufferResource&) = delete;
    OpenGLBufferResource(OpenGLBufferResource&&) noexcept = default;
    OpenGLBufferResource& operator=(const OpenGLBufferResource&) = delete;
    OpenGLBufferResource& operator=(OpenGLBufferResource&& a_other) noexcept;

    QOpenGLBuffer* Create(QOpenGLBuffer::Type a_type);
    void DestroyWithCurrentContext();

    [[nodiscard]] QOpenGLBuffer* Get() const noexcept {
        return _buffer.get();
    }
    [[nodiscard]] explicit operator bool() const noexcept {
        return _buffer != nullptr;
    }
    [[nodiscard]] QOpenGLBuffer* operator->() const noexcept {
        return _buffer.get();
    }

private:
    std::unique_ptr<QOpenGLBuffer> _buffer;
};

class OpenGLVertexArrayResource final {
public:
    OpenGLVertexArrayResource() = default;
    ~OpenGLVertexArrayResource();
    OpenGLVertexArrayResource(const OpenGLVertexArrayResource&) = delete;
    OpenGLVertexArrayResource(OpenGLVertexArrayResource&&) noexcept = default;
    OpenGLVertexArrayResource& operator=(const OpenGLVertexArrayResource&) = delete;
    OpenGLVertexArrayResource& operator=(OpenGLVertexArrayResource&& a_other) noexcept;

    QOpenGLVertexArrayObject* Create();
    void DestroyWithCurrentContext();

    [[nodiscard]] QOpenGLVertexArrayObject* Get() const noexcept {
        return _vertexArray.get();
    }
    [[nodiscard]] explicit operator bool() const noexcept {
        return _vertexArray != nullptr;
    }

private:
    std::unique_ptr<QOpenGLVertexArrayObject> _vertexArray;
};

class OpenGLTextureResource final {
public:
    OpenGLTextureResource() = default;
    OpenGLTextureResource(GLuint a_textureId, GLenum a_target);
    ~OpenGLTextureResource();
    OpenGLTextureResource(const OpenGLTextureResource&) = delete;
    OpenGLTextureResource(OpenGLTextureResource&& a_other) noexcept;
    OpenGLTextureResource& operator=(const OpenGLTextureResource&) = delete;
    OpenGLTextureResource& operator=(OpenGLTextureResource&& a_other) noexcept;

    void Set(GLuint a_textureId, GLenum a_target);
    [[nodiscard]] GLuint Release() noexcept;
    void DestroyWithCurrentContext(QOpenGLFunctions_2_1* a_f = nullptr);
    void Bind(int a_textureUnit) const;
    void Bind(QOpenGLFunctions_2_1* a_f) const;

    [[nodiscard]] bool IsCreated() const noexcept {
        return _textureId != 0;
    }
    [[nodiscard]] GLuint Id() const noexcept {
        return _textureId;
    }
    [[nodiscard]] GLenum Target() const noexcept {
        return _target;
    }
    [[nodiscard]] explicit operator bool() const noexcept {
        return IsCreated();
    }

private:
    GLuint _textureId = 0;
    GLenum _target = 0;
};

class QtOpenGLTextureResource final {
public:
    QtOpenGLTextureResource() = default;
    explicit QtOpenGLTextureResource(QOpenGLTexture* a_texture);
    ~QtOpenGLTextureResource();
    QtOpenGLTextureResource(const QtOpenGLTextureResource&) = delete;
    QtOpenGLTextureResource(QtOpenGLTextureResource&&) noexcept = default;
    QtOpenGLTextureResource& operator=(const QtOpenGLTextureResource&) = delete;
    QtOpenGLTextureResource& operator=(QtOpenGLTextureResource&& a_other) noexcept;

    void DestroyWithCurrentContext();

    [[nodiscard]] QOpenGLTexture* Get() const noexcept {
        return _texture.get();
    }
    [[nodiscard]] explicit operator bool() const noexcept {
        return _texture != nullptr;
    }
    [[nodiscard]] QOpenGLTexture* operator->() const noexcept {
        return _texture.get();
    }

private:
    std::unique_ptr<QOpenGLTexture> _texture;
};
