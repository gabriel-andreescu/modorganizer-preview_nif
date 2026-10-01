#pragma once

#include "Render/Camera.h"
#include "Render/Resources.h"
#include "Render/ShaderManager.h"
#include "Render/Shape.h"
#include "Textures/Manager.h"
#include "Textures/Source.h"

#include <QOpenGLFunctions_2_1>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
#include <QSharedPointer>

#include <nifly/NifFile.hpp>
#include <uibase/imoinfo.h>

#include <memory>

class OpenGLCollisionOverlay;

struct SkippedShape {
    QString name;
    QString reason;
};

class NifWidget final : public QOpenGLWidget {
    Q_OBJECT

public:
    NifWidget(
        std::shared_ptr<nifly::NifFile> a_nifFile,
        const QString& a_dataPath,
        MOBase::IOrganizer* a_organizer,
        QSharedPointer<Camera> a_camera,
        TextureSourceProvider a_textureSource,
        QWidget* a_parent = nullptr
    );

    ~NifWidget() override;
    NifWidget(const NifWidget&) = delete;
    NifWidget(NifWidget&&) = delete;
    NifWidget& operator=(const NifWidget&) = delete;
    NifWidget& operator=(NifWidget&&) = delete;

    [[nodiscard]] QSharedPointer<Camera> SharedCamera() const {
        return _camera;
    }
    [[nodiscard]] const std::vector<OpenGLShape>& Shapes() const noexcept {
        return _glShapes;
    }
    [[nodiscard]] const std::vector<OpenGLShape>& EditorMarkerShapes() const noexcept {
        return _editorMarkerShapes;
    }
    [[nodiscard]] const std::vector<SkippedShape>& SkippedShapes() const noexcept {
        return _skippedShapes;
    }
    [[nodiscard]] const nifly::BoundingSphere& SceneBounds() const noexcept {
        return _sceneBounds;
    }
    [[nodiscard]] const TextureManager& Textures() const noexcept {
        return *_textureManager;
    }
    [[nodiscard]] const ShaderManager& Shaders() const noexcept {
        return *_shaderManager;
    }
    // Loads the shapes and GL state of a widget that has not been painted yet.
    void EnsureInitialized();
    // Renders the current scene into an offscreen image of the given size, independent of the widget's size.
    [[nodiscard]] QImage RenderImage(QSize a_size);
    void SetCamera(QSharedPointer<Camera> a_camera);
    void SetShowCollision(bool a_showCollision);
    void SetShowEditorMarkers(bool a_showEditorMarkers);
    void ResetCamera();

protected:
    void mousePressEvent(QMouseEvent* a_event) override;
    void mouseMoveEvent(QMouseEvent* a_event) override;
    void wheelEvent(QWheelEvent* a_event) override;

    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int a_w, int a_h) override;

private:
    [[nodiscard]] float AspectRatio() const;
    [[nodiscard]] std::vector<const OpenGLShape*> DrawnShapes() const;
    void Cleanup();
    void CopySceneColorTexture(QOpenGLFunctions_2_1* a_f);
    void EnsureCollisionOverlay();
    void EnsureSceneColorTexture(QOpenGLFunctions_2_1* a_f);
    void FrameCameraIfNeeded();
    void ReleaseSceneColorTexture(QOpenGLFunctions_2_1* a_f);
    void RenderCollisionOverlay();
    void RenderRefractionProxyPass(QOpenGLFunctions_2_1* a_f);
    void SetProjectionMatrix();
    void UpdateCamera();

    std::shared_ptr<nifly::NifFile> _nifFile;
    bool _isGrassModel = false;
    MOBase::IOrganizer* _moInfo = nullptr;

    std::unique_ptr<TextureManager> _textureManager;
    std::unique_ptr<ShaderManager> _shaderManager;

    std::vector<OpenGLShape> _glShapes;
    std::vector<OpenGLShape> _editorMarkerShapes;
    std::vector<SkippedShape> _skippedShapes;
    nifly::BoundingSphere _sceneBounds;
    std::unique_ptr<OpenGLCollisionOverlay> _collisionOverlay;
    bool _collisionOverlayBuildAttempted = false;
    bool _showCollision = false;
    bool _showEditorMarkers = false;

    QSharedPointer<Camera> _camera;
    QMetaObject::Connection _cameraConnection;

    QMatrix4x4 _viewMatrix;
    QMatrix4x4 _projectionMatrix;

    OpenGLTextureResource _sceneColorTexture;
    int _sceneColorTextureWidth = 0;
    int _sceneColorTextureHeight = 0;

    float _viewportWidth {};
    float _viewportHeight {};
    QPointF _mousePos;
};
