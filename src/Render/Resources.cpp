#include "Render/Resources.h"

#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLFunctions_2_1>
#include <QOpenGLVersionFunctionsFactory>

#include <utility>

namespace {
QOpenGLFunctions_2_1* CurrentOpenGLFunctions() {
    return QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_2_1>(QOpenGLContext::currentContext());
}
} // namespace

OpenGLBufferResource::~OpenGLBufferResource() {
    if (_buffer && _buffer->isCreated()) {
        qWarning("Leaking OpenGL buffer: DestroyWithCurrentContext() was not called");
        [[maybe_unused]] const auto* const leakedBuffer = _buffer.release();
    }
}

OpenGLBufferResource& OpenGLBufferResource::operator=(OpenGLBufferResource&& a_other) noexcept {
    if (this != &a_other) {
        if (_buffer && _buffer->isCreated()) {
            qWarning("Leaking OpenGL buffer: DestroyWithCurrentContext() was not called before move assignment");
            [[maybe_unused]] const auto* const leakedBuffer = _buffer.release();
        }
        _buffer = std::move(a_other._buffer);
    }
    return *this;
}

QOpenGLBuffer* OpenGLBufferResource::Create(const QOpenGLBuffer::Type a_type) {
    if (_buffer && _buffer->isCreated()) {
        qWarning("Replacing live OpenGL buffer without DestroyWithCurrentContext()");
        [[maybe_unused]] const auto* const leakedBuffer = _buffer.release();
    }

    _buffer = std::make_unique<QOpenGLBuffer>(a_type);
    return _buffer.get();
}

void OpenGLBufferResource::DestroyWithCurrentContext() {
    if (!_buffer) {
        return;
    }

    if (_buffer->isCreated()) {
        _buffer->destroy();
    }
    _buffer.reset();
}

OpenGLVertexArrayResource::~OpenGLVertexArrayResource() {
    if (_vertexArray && _vertexArray->isCreated()) {
        qWarning("Leaking OpenGL vertex array: DestroyWithCurrentContext() was not called");
        [[maybe_unused]] const auto* const leakedVertexArray = _vertexArray.release();
    }
}

OpenGLVertexArrayResource& OpenGLVertexArrayResource::operator=(OpenGLVertexArrayResource&& a_other) noexcept {
    if (this != &a_other) {
        if (_vertexArray && _vertexArray->isCreated()) {
            qWarning("Leaking OpenGL vertex array: DestroyWithCurrentContext() was not called before move assignment");
            [[maybe_unused]] const auto* const leakedVertexArray = _vertexArray.release();
        }
        _vertexArray = std::move(a_other._vertexArray);
    }
    return *this;
}

QOpenGLVertexArrayObject* OpenGLVertexArrayResource::Create() {
    if (_vertexArray && _vertexArray->isCreated()) {
        qWarning("Replacing live OpenGL vertex array without DestroyWithCurrentContext()");
        [[maybe_unused]] const auto* const leakedVertexArray = _vertexArray.release();
    }

    _vertexArray = std::make_unique<QOpenGLVertexArrayObject>();
    return _vertexArray.get();
}

void OpenGLVertexArrayResource::DestroyWithCurrentContext() {
    if (!_vertexArray) {
        return;
    }

    if (_vertexArray->isCreated()) {
        _vertexArray->destroy();
    }
    _vertexArray.release()->deleteLater();
}

OpenGLTextureResource::OpenGLTextureResource(const GLuint a_textureId, const GLenum a_target)
    : _textureId(a_textureId)
    , _target(a_target) {}

OpenGLTextureResource::~OpenGLTextureResource() {
    if (_textureId != 0) {
        qWarning("Leaking OpenGL texture %u: DestroyWithCurrentContext() was not called", _textureId);
    }
}

OpenGLTextureResource::OpenGLTextureResource(OpenGLTextureResource&& a_other) noexcept
    : _textureId(a_other._textureId)
    , _target(a_other._target) {
    a_other._textureId = 0;
    a_other._target = 0;
}

OpenGLTextureResource& OpenGLTextureResource::operator=(OpenGLTextureResource&& a_other) noexcept {
    if (this != &a_other) {
        if (_textureId != 0) {
            qWarning(
                "Leaking OpenGL texture %u: DestroyWithCurrentContext() was not called before move assignment",
                _textureId
            );
        }
        _textureId = a_other._textureId;
        _target = a_other._target;
        a_other._textureId = 0;
        a_other._target = 0;
    }
    return *this;
}

void OpenGLTextureResource::Set(const GLuint a_textureId, const GLenum a_target) {
    if (_textureId != 0) {
        qWarning("Replacing live OpenGL texture %u without DestroyWithCurrentContext()", _textureId);
    }

    _textureId = a_textureId;
    _target = a_target;
}

GLuint OpenGLTextureResource::Release() noexcept {
    const auto textureId = _textureId;
    _textureId = 0;
    _target = 0;
    return textureId;
}

void OpenGLTextureResource::DestroyWithCurrentContext(QOpenGLFunctions_2_1* a_f) {
    if (_textureId == 0) {
        return;
    }

    if (!a_f) {
        a_f = CurrentOpenGLFunctions();
    }
    if (!a_f) {
        qWarning("Leaking OpenGL texture %u: OpenGL 2.1 functions unavailable", _textureId);
        return;
    }

    const auto textureId = _textureId;
    a_f->glDeleteTextures(1, &textureId);
    _textureId = 0;
    _target = 0;
}

void OpenGLTextureResource::Bind(const int a_textureUnit) const {
    if (_textureId == 0) {
        return;
    }

    if (auto* const functions = CurrentOpenGLFunctions()) {
        functions->glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(a_textureUnit));
        Bind(functions);
    }
}

void OpenGLTextureResource::Bind(QOpenGLFunctions_2_1* a_f) const {
    if (a_f && _textureId != 0) {
        a_f->glBindTexture(_target, _textureId);
    }
}

QtOpenGLTextureResource::QtOpenGLTextureResource(QOpenGLTexture* a_texture)
    : _texture(a_texture) {}

QtOpenGLTextureResource::~QtOpenGLTextureResource() {
    if (_texture && _texture->isCreated()) {
        qWarning("Leaking QOpenGLTexture: DestroyWithCurrentContext() was not called");
        [[maybe_unused]] const auto* const leakedTexture = _texture.release();
    }
}

QtOpenGLTextureResource& QtOpenGLTextureResource::operator=(QtOpenGLTextureResource&& a_other) noexcept {
    if (this != &a_other) {
        if (_texture && _texture->isCreated()) {
            qWarning("Leaking QOpenGLTexture: DestroyWithCurrentContext() was not called before move assignment");
            [[maybe_unused]] const auto* const leakedTexture = _texture.release();
        }
        _texture = std::move(a_other._texture);
    }
    return *this;
}

void QtOpenGLTextureResource::DestroyWithCurrentContext() {
    if (!_texture) {
        return;
    }

    if (_texture->isCreated()) {
        _texture->destroy();
    }
    _texture.reset();
}
