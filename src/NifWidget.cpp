#include "NifWidget.h"
#include "CollisionGeometry.h"
#include "NifExtensions.h"
#include "OpenGLCollisionOverlay.h"

#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLFunctions_2_1>
#include <QOpenGLVersionFunctionsFactory>
#include <QWheelEvent>
#include <algorithm>
#include <exception>
#include <utility>

namespace {
constexpr int SceneTextureUnit = 0;

QSharedPointer<Camera> makeCamera() {
    return {new Camera(), &Camera::deleteLater};
}
} // namespace

NifWidget::NifWidget(
    std::shared_ptr<nifly::NifFile> nifFile,
    MOBase::IOrganizer* organizer,
    QSharedPointer<Camera> camera,
    TextureSourceProvider textureSource,
    const bool debugContext,
    QWidget* parent,
    const Qt::WindowFlags f
)
    : QOpenGLWidget(parent, f)
    , m_NifFile {std::move(nifFile)}
    , m_MOInfo {organizer}
    , m_TextureManager {std::make_unique<TextureManager>(organizer, std::move(textureSource))}
    , m_ShaderManager {std::make_unique<ShaderManager>(organizer)} {
    setCamera(std::move(camera));

    QSurfaceFormat format;
    format.setVersion(2, 1);

    if (debugContext) {
        format.setOption(QSurfaceFormat::DebugContext);
        m_Context = new QOpenGLContext();
        m_Context->setFormat(format);
        m_Context->create();
    }

    setFormat(format);
}

NifWidget::~NifWidget() {
    cleanup();
}

void NifWidget::mousePressEvent(QMouseEvent* event) {
    m_MousePos = event->globalPosition();
}

void NifWidget::mouseMoveEvent(QMouseEvent* event) {
    const auto pos = event->globalPosition();
    const auto delta = pos - m_MousePos;
    m_MousePos = pos;

    switch (event->buttons()) {
        case Qt::LeftButton: {
            m_Camera->rotate(static_cast<float>(delta.x() * 0.5f), static_cast<float>(delta.y() * 0.5f));
        } break;
        case Qt::MiddleButton: {
            const float viewDX = m_Camera->distance() / m_ViewportWidth;
            const float viewDY = m_Camera->distance() / m_ViewportHeight;

            QMatrix4x4 r;
            r.rotate(-m_Camera->yaw(), 0.0f, 1.0f, 0.0f);
            r.rotate(-m_Camera->pitch(), 1.0f, 0.0f, 0.0f);

            const auto pan = r
                             * QVector4D(
                                 static_cast<float>(-delta.x() * viewDX),
                                 static_cast<float>(delta.y() * viewDY),
                                 0.0f,
                                 0.0f
                             );

            m_Camera->pan(QVector3D(pan));
        } break;

        case Qt::RightButton: {
            if (event->modifiers() == Qt::ShiftModifier) {
                m_Camera->zoomDistance(static_cast<float>(delta.y() * 0.1f));
            }
        } break;
        default:;
    }
}

void NifWidget::wheelEvent(QWheelEvent* event) {
    m_Camera->zoomFactor(1.0f - (static_cast<float>(event->angleDelta().y()) / 120.0f * 0.38f));
}

void NifWidget::setCamera(QSharedPointer<Camera> camera) {
    if (camera.isNull()) {
        camera = makeCamera();
    }

    if (m_Camera == camera) {
        return;
    }

    if (m_CameraConnection) {
        disconnect(m_CameraConnection);
    }

    m_Camera = std::move(camera);
    m_CameraConnection = connect(m_Camera.get(), &Camera::cameraMoved, this, [this]() {
        updateCamera();
        update();
    });

    updateCamera();
    setProjectionMatrix();
    update();
}

void NifWidget::setShowCollision(const bool showCollision) {
    if (m_ShowCollision == showCollision) {
        return;
    }

    m_ShowCollision = showCollision;
    update();
}

void NifWidget::resetCamera() {
    if (!m_Camera) {
        return;
    }

    float largestRadius = 0.0f;
    QVector3D lookAt;
    for (const auto& shape : m_GLShapes) {
        if (shape.isRefractionProxy) {
            continue;
        }

        if (shape.bounds.radius > largestRadius) {
            largestRadius = shape.bounds.radius;
            lookAt = {-shape.bounds.center.x, shape.bounds.center.z, shape.bounds.center.y};
        }
    }

    if (largestRadius > 0.0f) {
        m_Camera->setState({.lookAt = lookAt, .pitch = 0.0f, .yaw = 0.0f, .distance = largestRadius * 2.4f});
    }
}

void NifWidget::messageLogged(const QOpenGLDebugMessage& message) {
    const auto msg = tr("OpenGL debug message: %1").arg(message.message());
    qDebug(qUtf8Printable(msg));
}

void NifWidget::initializeGL() {
    if (m_Context) {
        m_Logger = new QOpenGLDebugLogger(m_Context);
        if (m_Logger->initialize()) {
            m_Logger->enableMessages();
            qDebug() << "GL_DEBUG Debug Logger" << m_Logger;
            connect(m_Logger, &QOpenGLDebugLogger::messageLogged, this, &NifWidget::messageLogged);
            m_Logger->startLogging();
        }
    }

    auto shapes = m_NifFile->GetShapes();
    for (auto& shape : shapes) {
        if (!shape) {
            continue;
        }
        if (shape->flags & TriShape::Hidden) {
            continue;
        }

        try {
            m_GLShapes.emplace_back(m_NifFile.get(), shape, m_TextureManager.get());
        } catch (const std::exception& e) {
            qWarning("Failed to prepare NIF shape for preview: %s", e.what());
        } catch (...) {
            qWarning("Failed to prepare NIF shape for preview: unknown exception");
        }
    }

    frameCameraIfNeeded();
    updateCamera();

    auto* const f = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_2_1>(QOpenGLContext::currentContext());
    if (!f) {
        qWarning("NIF preview could not initialize OpenGL 2.1 functions");
        return;
    }

    f->glEnable(GL_DEPTH_TEST);
    f->glDepthFunc(GL_LEQUAL);
    f->glClearColor(0.18f, 0.18f, 0.18f, 1.0f);
}

void NifWidget::paintGL() {
    auto* const f = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_2_1>(QOpenGLContext::currentContext());
    if (!f) {
        return;
    }
    f->glDepthMask(GL_TRUE);
    f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const auto drawShape = [&](const OpenGLShape& shape) {
        if (auto* const program = m_ShaderManager->getProgram(shape.shaderType);
            program && program->isLinked() && program->bind()) {
            auto binder = QOpenGLVertexArrayObject::Binder(shape.vertexArray.get());

            const auto& modelMatrix = shape.modelMatrix;
            auto modelViewMatrix = m_ViewMatrix * modelMatrix;
            auto mvpMatrix = m_ProjectionMatrix * modelViewMatrix;

            program->setUniformValue("worldMatrix", modelMatrix);
            program->setUniformValue("viewMatrix", m_ViewMatrix);
            program->setUniformValue("modelViewMatrix", modelViewMatrix);
            program->setUniformValue("modelViewMatrixInverse", modelViewMatrix.inverted());
            program->setUniformValue("normalMatrix", modelViewMatrix.normalMatrix());
            program->setUniformValue("mvpMatrix", mvpMatrix);
            program->setUniformValue("lightDirection", QVector3D(0, 0, 1));

            shape.setupShaders(program);

            if (shape.indexBuffer && shape.indexBuffer->isCreated()) {
                shape.indexBuffer->bind();
                f->glDrawElements(GL_TRIANGLES, shape.elements, GL_UNSIGNED_SHORT, nullptr);
                shape.indexBuffer->release();
            }

            program->release();
        }
    };

    f->glEnable(GL_POLYGON_OFFSET_FILL);
    f->glPolygonOffset(1.0f, 2.0f);

    for (const auto& shape : m_GLShapes) {
        if (!shape.isRefractionProxy && !shape.usesAlphaPass()) {
            drawShape(shape);
        }
    }

    f->glDisable(GL_POLYGON_OFFSET_FILL);

    bool hasRefractionProxy = false;
    for (const auto& shape : m_GLShapes) {
        if (shape.isRefractionProxy) {
            hasRefractionProxy = true;
            continue;
        }

        if (shape.usesAlphaPass()) {
            drawShape(shape);
        }
    }

    if (hasRefractionProxy) {
        copySceneColorTexture(f);
        renderRefractionProxyPass(f);
    }

    renderCollisionOverlay();

    f->glDepthMask(GL_TRUE);
}

void NifWidget::resizeGL(const int w, const int h) {
    m_ViewportWidth = static_cast<float>(w);
    m_ViewportHeight = static_cast<float>(h);

    setProjectionMatrix();
}

void NifWidget::cleanup() {
    if (!context()) {
        return;
    }

    makeCurrent();

    for (auto& shape : m_GLShapes) {
        shape.destroy();
    }
    m_GLShapes.clear();

    if (m_CollisionOverlay) {
        m_CollisionOverlay->destroy();
        m_CollisionOverlay.reset();
    }
    m_CollisionOverlayBuildAttempted = false;

    auto* const f = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_2_1>(QOpenGLContext::currentContext());
    releaseSceneColorTexture(f);

    m_TextureManager->cleanup();
}

void NifWidget::copySceneColorTexture(QOpenGLFunctions_2_1* f) {
    ensureSceneColorTexture(f);
    if (!f || !m_SceneColorTexture) {
        return;
    }

    f->glActiveTexture(GL_TEXTURE0 + SceneTextureUnit);
    m_SceneColorTexture.bind(f);
    f->glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, m_SceneColorTextureWidth, m_SceneColorTextureHeight);
}

void NifWidget::ensureSceneColorTexture(QOpenGLFunctions_2_1* f) {
    if (!f) {
        return;
    }

    const auto textureWidth = std::max(1, static_cast<int>(m_ViewportWidth));
    const auto textureHeight = std::max(1, static_cast<int>(m_ViewportHeight));
    if (m_SceneColorTexture && m_SceneColorTextureWidth == textureWidth && m_SceneColorTextureHeight == textureHeight) {
        return;
    }

    releaseSceneColorTexture(f);

    GLuint textureId = 0;
    f->glGenTextures(1, &textureId);
    if (textureId == 0) {
        qWarning("Failed to create scene color texture for refraction preview");
        return;
    }
    m_SceneColorTexture.set(textureId, GL_TEXTURE_2D);

    m_SceneColorTextureWidth = textureWidth;
    m_SceneColorTextureHeight = textureHeight;

    f->glActiveTexture(GL_TEXTURE0 + SceneTextureUnit);
    m_SceneColorTexture.bind(f);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    f->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textureWidth, textureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
}

void NifWidget::ensureCollisionOverlay() {
    if (m_CollisionOverlay || m_CollisionOverlayBuildAttempted) {
        return;
    }

    m_CollisionOverlayBuildAttempted = true;
    try {
        m_CollisionOverlay = std::make_unique<OpenGLCollisionOverlay>(CollisionGeometryBuilder::build(m_NifFile.get()));
    } catch (const std::exception& e) {
        qWarning("Failed to prepare NIF collision overlay: %s", e.what());
    } catch (...) {
        qWarning("Failed to prepare NIF collision overlay: unknown exception");
    }
}

void NifWidget::releaseSceneColorTexture(QOpenGLFunctions_2_1* f) {
    m_SceneColorTexture.destroyWithCurrentContext(f);
    m_SceneColorTextureWidth = 0;
    m_SceneColorTextureHeight = 0;
}

void NifWidget::frameCameraIfNeeded() {
    if (!m_Camera || m_Camera->hasState()) {
        return;
    }

    resetCamera();
}

void NifWidget::setProjectionMatrix() {
    if (!m_Camera) {
        return;
    }

    const auto viewportWidth = m_ViewportWidth > 0.0f ? m_ViewportWidth : 1.0f;
    const auto viewportHeight = m_ViewportHeight > 0.0f ? m_ViewportHeight : 1.0f;

    QMatrix4x4 m;
    m.perspective(40.0f, viewportWidth / viewportHeight, m_Camera->nearPlane(), m_Camera->farPlane());
    m_ProjectionMatrix = m;
}

void NifWidget::updateCamera() {
    if (!m_Camera) {
        return;
    }

    QMatrix4x4 m;
    m.translate(0.0f, 0.0f, -m_Camera->distance());
    m.rotate(m_Camera->pitch(), 1.0f, 0.0f, 0.0f);
    m.rotate(m_Camera->yaw(), 0.0f, 1.0f, 0.0f);
    m.translate(-m_Camera->lookAt());
    m *= QMatrix4x4 {
        -1,
        0,
        0,
        0,
        0,
        0,
        1,
        0,
        0,
        1,
        0,
        0,
        0,
        0,
        0,
        1,
    };
    m_ViewMatrix = m;

    setProjectionMatrix();
}

void NifWidget::renderCollisionOverlay() {
    if (!m_ShowCollision) {
        return;
    }

    ensureCollisionOverlay();
    if (!m_CollisionOverlay || m_CollisionOverlay->empty()) {
        return;
    }

    m_CollisionOverlay
        ->render(m_ShaderManager->getProgram(ShaderManager::CollisionWire), m_ViewMatrix, m_ProjectionMatrix);
}

void NifWidget::renderRefractionProxyPass(QOpenGLFunctions_2_1* f) {
    if (!f || !m_SceneColorTexture) {
        return;
    }

    auto* program = m_ShaderManager->getProgram(ShaderManager::SKRefractionProxy);
    if (!program || !program->isLinked()) {
        return;
    }

    f->glDisable(GL_POLYGON_OFFSET_FILL);
    f->glDisable(GL_BLEND);
    f->glDepthMask(GL_FALSE);

    for (const auto& shape : m_GLShapes) {
        if (!shape.isRefractionProxy || !program->bind()) {
            continue;
        }

        auto binder = QOpenGLVertexArrayObject::Binder(shape.vertexArray.get());

        const auto& modelMatrix = shape.modelMatrix;
        auto modelViewMatrix = m_ViewMatrix * modelMatrix;
        auto mvpMatrix = m_ProjectionMatrix * modelViewMatrix;

        program->setUniformValue("worldMatrix", modelMatrix);
        program->setUniformValue("viewMatrix", m_ViewMatrix);
        program->setUniformValue("modelViewMatrix", modelViewMatrix);
        program->setUniformValue("modelViewMatrixInverse", modelViewMatrix.inverted());
        program->setUniformValue("normalMatrix", modelViewMatrix.normalMatrix());
        program->setUniformValue("mvpMatrix", mvpMatrix);
        program->setUniformValue("lightDirection", QVector3D(0, 0, 1));

        shape.setupShaders(program);

        f->glDisable(GL_BLEND);
        f->glDepthMask(GL_FALSE);
        f->glActiveTexture(GL_TEXTURE0 + SceneTextureUnit);
        m_SceneColorTexture.bind(f);

        program->setUniformValue("SceneMap", SceneTextureUnit);
        program->setUniformValue(
            "viewportSize",
            QVector2D(static_cast<float>(m_SceneColorTextureWidth), static_cast<float>(m_SceneColorTextureHeight))
        );
        program->setUniformValue("refractionStrength", std::clamp(shape.refractionStrength, 0.0f, 1.0f));

        if (shape.indexBuffer && shape.indexBuffer->isCreated()) {
            shape.indexBuffer->bind();
            f->glDrawElements(GL_TRIANGLES, shape.elements, GL_UNSIGNED_SHORT, nullptr);
            shape.indexBuffer->release();
        }

        program->release();
    }
}
