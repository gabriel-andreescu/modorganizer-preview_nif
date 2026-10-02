#include "Render/NifWidget.h"
#include "Nif/CollisionGeometry.h"
#include "Nif/GrassModel.h"
#include "Nif/ShaderFlags.h"
#include "Nif/ShapeVisibility.h"
#include "Render/CollisionOverlay.h"
#include "Render/DrawOrder.h"
#include "Render/Framing.h"

#include <QDebug>
#include <QImage>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QOpenGLFunctions_2_1>
#include <QOpenGLVersionFunctionsFactory>
#include <QWheelEvent>
#include <algorithm>
#include <exception>
#include <utility>

namespace {
constexpr int kSceneTextureUnit = 0;

QSharedPointer<Camera> MakeCamera() {
    return {new Camera(), &Camera::deleteLater};
}
} // namespace

NifWidget::NifWidget(
    std::shared_ptr<nifly::NifFile> a_nifFile,
    const QString& a_dataPath,
    std::shared_ptr<const GameFiles> a_gameFiles,
    QSharedPointer<Camera> a_camera,
    TextureSourceProvider a_textureSource,
    QWidget* a_parent
)
    : QOpenGLWidget(a_parent)
    , _nifFile {std::move(a_nifFile)}
    , _isGrassModel {IsGrassModelPath(a_dataPath)}
    , _gameFiles {std::move(a_gameFiles)}
    , _textureManager {std::make_unique<TextureManager>(*_gameFiles, std::move(a_textureSource))}
    , _shaderManager {std::make_unique<ShaderManager>()} {
    SetCamera(std::move(a_camera));

    QSurfaceFormat format;
    format.setVersion(2, 1);
    setFormat(format);
}

NifWidget::~NifWidget() {
    Cleanup();
}

void NifWidget::mousePressEvent(QMouseEvent* a_event) {
    _mousePos = a_event->globalPosition();
}

void NifWidget::mouseMoveEvent(QMouseEvent* a_event) {
    const auto pos = a_event->globalPosition();
    const auto delta = pos - _mousePos;
    _mousePos = pos;

    switch (a_event->buttons()) {
        case Qt::LeftButton: {
            _camera->Rotate(static_cast<float>(delta.x() * 0.5F), static_cast<float>(delta.y() * 0.5F));
        } break;
        case Qt::MiddleButton: {
            const float viewDX = _camera->Distance() / _viewportWidth;
            const float viewDY = _camera->Distance() / _viewportHeight;

            QMatrix4x4 rotation;
            rotation.rotate(-_camera->Yaw(), 0.0F, 1.0F, 0.0F);
            rotation.rotate(-_camera->Pitch(), 1.0F, 0.0F, 0.0F);

            const auto pan = rotation
                             * QVector4D(
                                 static_cast<float>(-delta.x() * viewDX),
                                 static_cast<float>(delta.y() * viewDY),
                                 0.0F,
                                 0.0F
                             );

            _camera->Pan(QVector3D(pan));
        } break;

        case Qt::RightButton: {
            if (a_event->modifiers() == Qt::ShiftModifier) {
                _camera->ZoomDistance(static_cast<float>(delta.y() * 0.1F));
            }
        } break;
        default:;
    }
}

void NifWidget::wheelEvent(QWheelEvent* a_event) {
    _camera->ZoomFactor(1.0F - (static_cast<float>(a_event->angleDelta().y()) / 120.0F * 0.38F));
}

void NifWidget::SetCamera(QSharedPointer<Camera> a_camera) {
    if (a_camera.isNull()) {
        a_camera = MakeCamera();
    }

    if (_camera == a_camera) {
        return;
    }

    if (_cameraConnection) {
        disconnect(_cameraConnection);
    }

    _camera = std::move(a_camera);
    _cameraConnection = connect(_camera.get(), &Camera::CameraMoved, this, [this] {
        UpdateCamera();
        update();
    });

    UpdateCamera();
    SetProjectionMatrix();
    update();
}

void NifWidget::EnsureInitialized() {
    if (!isValid()) {
        // Grabbing initializes the GL context and shapes of a widget that has not been painted yet.
        static_cast<void>(grabFramebuffer());
    }
}

QImage NifWidget::RenderImage(const QSize a_size) {
    EnsureInitialized();
    makeCurrent();
    QOpenGLFramebufferObjectFormat format;
    format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    QOpenGLFramebufferObject framebuffer(a_size, format);
    framebuffer.bind();

    const auto viewportWidth = _viewportWidth;
    const auto viewportHeight = _viewportHeight;
    resizeGL(a_size.width(), a_size.height());
    if (auto* const functions = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_2_1>(context())) {
        functions->glViewport(0, 0, a_size.width(), a_size.height());
    }
    paintGL();
    auto image = framebuffer.toImage();
    // The widget shows the framebuffer's color without its alpha, which blended passes leave below 1.
    // Saving that alpha would make viewers blend refraction and alpha-tested cards with their background.
    image.reinterpretAsFormat(QImage::Format_RGB32);

    framebuffer.release();
    resizeGL(static_cast<int>(viewportWidth), static_cast<int>(viewportHeight));
    doneCurrent();
    return image;
}

void NifWidget::SetShowCollision(const bool a_showCollision) {
    if (_showCollision == a_showCollision) {
        return;
    }

    _showCollision = a_showCollision;
    update();
}

void NifWidget::SetShowEditorMarkers(const bool a_showEditorMarkers) {
    if (_showEditorMarkers == a_showEditorMarkers) {
        return;
    }

    _showEditorMarkers = a_showEditorMarkers;
    UpdateSceneBounds();
    if (_camera && _camera->IsFramed()) {
        ResetCamera();
    }
    update();
}

bool NifWidget::DrawsCollision() const {
    return _showCollision || _glShapes.empty();
}

void NifWidget::UpdateSceneBounds() {
    _sceneBounds = ::SceneBounds(DrawnShapes());
    if (_glShapes.empty() && _collisionOverlay) {
        _sceneBounds = Enclose(_sceneBounds, _collisionOverlay->Bounds());
    }
}

std::vector<const OpenGLShape*> NifWidget::DrawnShapes() const {
    std::vector<const OpenGLShape*> shapes;
    shapes.reserve(_glShapes.size() + (_showEditorMarkers ? _editorMarkerShapes.size() : 0));
    for (const auto& shape : _glShapes) {
        shapes.push_back(&shape);
    }
    if (_showEditorMarkers) {
        for (const auto& shape : _editorMarkerShapes) {
            shapes.push_back(&shape);
        }
    }
    return shapes;
}

void NifWidget::ResetCamera() {
    if (!_camera) {
        return;
    }

    if (_sceneBounds.radius <= 0.0F) {
        return;
    }
    _camera->Frame({
        .lookAt = NifToViewSpace().map(ToQVector(_sceneBounds.center)),
        .distance = FitDistance(_sceneBounds.radius, AspectRatio()),
    });
}

float NifWidget::AspectRatio() const {
    const auto viewportWidth = _viewportWidth > 0.0F ? _viewportWidth : 1.0F;
    const auto viewportHeight = _viewportHeight > 0.0F ? _viewportHeight : 1.0F;
    return viewportWidth / viewportHeight;
}

void NifWidget::initializeGL() {
    const auto shapes = _nifFile->GetShapes();
    for (const auto& shape : shapes) {
        if (!shape) {
            continue;
        }
        const auto name = QString::fromStdString(shape->name.get());
        const auto visibility = ClassifyShapeVisibility(*_nifFile, shape);
        if (visibility == ShapeVisibility::kHidden) {
            _skippedShapes.push_back({.name = name, .reason = QStringLiteral("hidden")});
            continue;
        }
        if (visibility == ShapeVisibility::kInactiveSwitchChild) {
            _skippedShapes.push_back({.name = name, .reason = QStringLiteral("inactive switch child")});
            continue;
        }

        auto& target = visibility == ShapeVisibility::kEditorMarker ? _editorMarkerShapes : _glShapes;
        try {
            target.emplace_back(_nifFile.get(), shape, _isGrassModel, _textureManager.get());
        } catch (const std::exception& e) {
            qWarning("Failed to prepare NIF shape '%s' for preview: %s", qUtf8Printable(name), e.what());
            _skippedShapes.push_back({.name = name, .reason = QString::fromUtf8(e.what())});
        } catch (...) {
            qWarning("Failed to prepare NIF shape '%s' for preview: unknown exception", qUtf8Printable(name));
            _skippedShapes.push_back({.name = name, .reason = QStringLiteral("unknown exception")});
        }
    }

    _textureResults = CollectTextureResults(_glShapes, _editorMarkerShapes, *_textureManager);
    emit TexturesLoaded(_textureResults);

    // The collision overlay needs the GL context, which only initialization and painting have.
    if (_glShapes.empty()) {
        EnsureCollisionOverlay();
    }
    UpdateSceneBounds();
    FrameCameraIfNeeded();
    UpdateCamera();

    auto* const functions = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_2_1>(QOpenGLContext::currentContext());
    if (!functions) {
        qWarning("NIF preview could not initialize OpenGL 2.1 functions");
        return;
    }

    functions->glEnable(GL_DEPTH_TEST);
    functions->glDepthFunc(GL_LEQUAL);
    functions->glClearColor(0.18F, 0.18F, 0.18F, 1.0F);
}

void NifWidget::paintGL() {
    auto* const functions = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_2_1>(QOpenGLContext::currentContext());
    if (!functions) {
        return;
    }
    functions->glDepthMask(GL_TRUE);
    functions->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Shown collision can reach past the meshes, so the clip planes follow whether it is drawn.
    if (DrawsCollision()) {
        EnsureCollisionOverlay();
    }
    SetProjectionMatrix();

    const auto drawShape = [&](const OpenGLShape& a_shape) {
        if (auto* const program = _shaderManager->GetProgram(a_shape.ShaderType());
            program && program->isLinked() && program->bind()) {
            const auto& modelMatrix = a_shape.ModelMatrix();
            const auto modelViewMatrix = _viewMatrix * modelMatrix;
            const auto mvpMatrix = _projectionMatrix * modelViewMatrix;

            program->setUniformValue("worldMatrix", modelMatrix);
            program->setUniformValue("viewMatrix", _viewMatrix);
            program->setUniformValue("modelViewMatrix", modelViewMatrix);
            program->setUniformValue("modelViewMatrixInverse", modelViewMatrix.inverted());
            program->setUniformValue("normalMatrix", modelViewMatrix.normalMatrix());
            program->setUniformValue("mvpMatrix", mvpMatrix);
            program->setUniformValue("lightDirection", QVector3D(0, 0, 1));

            a_shape.SetupShaders(program);
            a_shape.Draw(functions);

            program->release();
        }
    };

    functions->glEnable(GL_POLYGON_OFFSET_FILL);
    functions->glPolygonOffset(1.0F, 2.0F);

    const auto drawnShapes = DrawnShapes();
    for (const auto* const shape : drawnShapes) {
        if (!shape->IsRefractionProxy() && !shape->UsesAlphaPass()) {
            drawShape(*shape);
        }
    }

    functions->glDisable(GL_POLYGON_OFFSET_FILL);

    for (const auto* const shape : BackToFrontAlphaShapes(drawnShapes, _viewMatrix)) {
        drawShape(*shape);
    }

    const bool hasRefractionProxy = std::ranges::any_of(_glShapes, &OpenGLShape::IsRefractionProxy);
    if (hasRefractionProxy) {
        CopySceneColorTexture(functions);
        RenderRefractionProxyPass(functions);
    }

    RenderCollisionOverlay();

    functions->glDepthMask(GL_TRUE);
}

void NifWidget::resizeGL(const int a_w, const int a_h) {
    _viewportWidth = static_cast<float>(a_w);
    _viewportHeight = static_cast<float>(a_h);

    if (_camera && _camera->IsFramed()) {
        _camera->Refit(FitDistance(_sceneBounds.radius, AspectRatio()));
    }
    SetProjectionMatrix();
}

void NifWidget::Cleanup() {
    if (!context()) {
        return;
    }

    makeCurrent();

    for (auto& shape : _glShapes) {
        shape.Destroy();
    }
    _glShapes.clear();
    for (auto& shape : _editorMarkerShapes) {
        shape.Destroy();
    }
    _editorMarkerShapes.clear();

    if (_collisionOverlay) {
        _collisionOverlay->Destroy();
        _collisionOverlay.reset();
    }
    _collisionOverlayBuildAttempted = false;

    auto* const functions = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_2_1>(QOpenGLContext::currentContext());
    ReleaseSceneColorTexture(functions);

    _textureManager->Cleanup();
}

void NifWidget::CopySceneColorTexture(QOpenGLFunctions_2_1* a_f) {
    EnsureSceneColorTexture(a_f);
    if (!a_f || !_sceneColorTexture) {
        return;
    }

    a_f->glActiveTexture(GL_TEXTURE0 + kSceneTextureUnit);
    _sceneColorTexture.Bind(a_f);
    a_f->glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, _sceneColorTextureWidth, _sceneColorTextureHeight);
}

void NifWidget::EnsureSceneColorTexture(QOpenGLFunctions_2_1* a_f) {
    if (!a_f) {
        return;
    }

    const auto textureWidth = std::max(1, static_cast<int>(_viewportWidth));
    const auto textureHeight = std::max(1, static_cast<int>(_viewportHeight));
    if (_sceneColorTexture && _sceneColorTextureWidth == textureWidth && _sceneColorTextureHeight == textureHeight) {
        return;
    }

    ReleaseSceneColorTexture(a_f);

    GLuint textureId = 0;
    a_f->glGenTextures(1, &textureId);
    if (textureId == 0) {
        qWarning("Failed to create scene color texture for refraction preview");
        return;
    }
    _sceneColorTexture.Set(textureId, GL_TEXTURE_2D);

    _sceneColorTextureWidth = textureWidth;
    _sceneColorTextureHeight = textureHeight;

    a_f->glActiveTexture(GL_TEXTURE0 + kSceneTextureUnit);
    _sceneColorTexture.Bind(a_f);
    a_f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    a_f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    a_f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    a_f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    a_f->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textureWidth, textureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
}

void NifWidget::EnsureCollisionOverlay() {
    if (_collisionOverlay || _collisionOverlayBuildAttempted) {
        return;
    }

    _collisionOverlayBuildAttempted = true;
    try {
        _collisionOverlay = std::make_unique<OpenGLCollisionOverlay>(CollisionGeometryBuilder::Build(_nifFile.get()));
    } catch (const std::exception& e) {
        qWarning("Failed to prepare NIF collision overlay: %s", e.what());
    } catch (...) {
        qWarning("Failed to prepare NIF collision overlay: unknown exception");
    }
}

void NifWidget::ReleaseSceneColorTexture(QOpenGLFunctions_2_1* a_f) {
    _sceneColorTexture.DestroyWithCurrentContext(a_f);
    _sceneColorTextureWidth = 0;
    _sceneColorTextureHeight = 0;
}

void NifWidget::FrameCameraIfNeeded() {
    if (!_camera || _camera->HasState()) {
        return;
    }

    ResetCamera();
}

void NifWidget::SetProjectionMatrix() {
    if (!_camera) {
        return;
    }

    auto drawnBounds = _sceneBounds;
    if (DrawsCollision() && _collisionOverlay) {
        drawnBounds = Enclose(drawnBounds, _collisionOverlay->Bounds());
    }
    const auto center = _viewMatrix.map(ToQVector(drawnBounds.center));
    const auto planes = FitClipPlanes(-center.z(), drawnBounds.radius);
    _projectionMatrix.setToIdentity();
    _projectionMatrix.perspective(kVerticalFieldOfView, AspectRatio(), planes.nearPlane, planes.farPlane);
}

void NifWidget::UpdateCamera() {
    if (!_camera) {
        return;
    }

    _viewMatrix.setToIdentity();
    _viewMatrix.translate(0.0F, 0.0F, -_camera->Distance());
    _viewMatrix.rotate(_camera->Pitch(), 1.0F, 0.0F, 0.0F);
    _viewMatrix.rotate(_camera->Yaw(), 0.0F, 1.0F, 0.0F);
    _viewMatrix.translate(-_camera->LookAt());
    _viewMatrix *= NifToViewSpace();

    SetProjectionMatrix();
}

void NifWidget::RenderCollisionOverlay() {
    if (!DrawsCollision()) {
        return;
    }

    if (!_collisionOverlay || _collisionOverlay->Empty()) {
        return;
    }

    _collisionOverlay
        ->Render(_shaderManager->GetProgram(ShaderManager::kCollisionWire), _viewMatrix, _projectionMatrix);
}

void NifWidget::RenderRefractionProxyPass(QOpenGLFunctions_2_1* a_f) {
    if (!a_f || !_sceneColorTexture) {
        return;
    }

    auto* program = _shaderManager->GetProgram(ShaderManager::kSKRefractionProxy);
    if (!program || !program->isLinked()) {
        return;
    }

    a_f->glDisable(GL_POLYGON_OFFSET_FILL);
    a_f->glDisable(GL_BLEND);
    a_f->glDepthMask(GL_FALSE);

    for (const auto& shape : _glShapes) {
        if (!shape.IsRefractionProxy() || !program->bind()) {
            continue;
        }

        const auto& modelMatrix = shape.ModelMatrix();
        const auto modelViewMatrix = _viewMatrix * modelMatrix;
        const auto mvpMatrix = _projectionMatrix * modelViewMatrix;

        program->setUniformValue("worldMatrix", modelMatrix);
        program->setUniformValue("viewMatrix", _viewMatrix);
        program->setUniformValue("modelViewMatrix", modelViewMatrix);
        program->setUniformValue("modelViewMatrixInverse", modelViewMatrix.inverted());
        program->setUniformValue("normalMatrix", modelViewMatrix.normalMatrix());
        program->setUniformValue("mvpMatrix", mvpMatrix);
        program->setUniformValue("lightDirection", QVector3D(0, 0, 1));

        shape.SetupShaders(program);

        a_f->glDisable(GL_BLEND);
        a_f->glDepthMask(GL_FALSE);
        a_f->glActiveTexture(GL_TEXTURE0 + kSceneTextureUnit);
        _sceneColorTexture.Bind(a_f);

        program->setUniformValue("SceneMap", kSceneTextureUnit);
        program->setUniformValue(
            "viewportSize",
            QVector2D(static_cast<float>(_sceneColorTextureWidth), static_cast<float>(_sceneColorTextureHeight))
        );
        program->setUniformValue("refractionStrength", std::clamp(shape.RefractionStrength(), 0.0F, 1.0F));
        shape.Draw(a_f);

        program->release();
    }
}
