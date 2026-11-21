#pragma once

#include "Animation/SceneAnimation.h"
#include "Nif/BoundingBox.h"
#include "Render/AxisView.h"
#include "Render/Camera.h"
#include "Render/OpeningView.h"
#include "Render/Resources.h"
#include "Render/ShaderManager.h"
#include "Render/Shape.h"
#include "Render/TextureResults.h"
#include "Textures/Manager.h"
#include "Textures/Selection.h"

#include <QOpenGLFunctions_2_1>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
#include <QSharedPointer>

#include <nifly/NifFile.hpp>

#include <memory>
#include <optional>

class GameFiles;
class OpenGLCollisionOverlay;
class ViewButtons;

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
        std::shared_ptr<const GameFiles> a_gameFiles,
        QSharedPointer<Camera> a_camera,
        TextureSelection a_textureSelection,
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
    [[nodiscard]] const BoundingBox& SceneBox() const noexcept {
        return _sceneBox;
    }
    [[nodiscard]] const TextureManager& Textures() const noexcept {
        return *_textureManager;
    }
    [[nodiscard]] const ShaderManager& Shaders() const noexcept {
        return *_shaderManager;
    }
    // False until the widget initializes, which TexturesLoaded announces.
    [[nodiscard]] bool HasMeshes() const noexcept {
        return !_glShapes.empty();
    }
    // Empty until the widget initializes, which TexturesLoaded announces.
    [[nodiscard]] const QVector<TextureResult>& TextureResults() const noexcept {
        return _textureResults;
    }
    // Loads the shapes and GL state of a widget that has not been painted yet.
    void EnsureInitialized();
    // Renders the current scene into an offscreen image of the given size, independent of the widget's size.
    [[nodiscard]] QImage RenderImage(QSize a_size);
    void SetCamera(QSharedPointer<Camera> a_camera);
    void SetShowCollision(bool a_showCollision);
    void SetShowEditorMarkers(bool a_showEditorMarkers);
    void ResetCamera();

    [[nodiscard]] bool IsAnimated() const noexcept {
        return _animation.IsAnimated();
    }
    [[nodiscard]] const Animation::SceneAnimation& MeshAnimation() const noexcept {
        return _animation;
    }
    // The moment of the animation the view shows, in seconds. Empty shows the mesh at rest.
    [[nodiscard]] std::optional<float> AnimationTime() const noexcept {
        return _animationTime;
    }
    void SetAnimationTime(std::optional<float> a_time);
    // The mesh's named animations, such as Open and Close, and the one that plays. Empty when it has none.
    [[nodiscard]] QStringList SequenceNames() const;
    [[nodiscard]] QString PlayingSequence() const;
    void PlaySequence(const QString& a_name);
    // The animated values at the shown moment. Empty at rest.
    [[nodiscard]] Animation::Pose AnimationPose() const;

signals:
    void TexturesLoaded(const QVector<TextureResult>& a_results);
    // A view button turned the camera to its view.
    void AxisViewShown();

protected:
    void mousePressEvent(QMouseEvent* a_event) override;
    void mouseMoveEvent(QMouseEvent* a_event) override;
    void wheelEvent(QWheelEvent* a_event) override;

    void resizeEvent(QResizeEvent* a_event) override;

    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int a_w, int a_h) override;

private:
    void ApplyAnimation();
    [[nodiscard]] float AspectRatio() const;
    // A NIF without meshes, such as a skeleton, has nothing to show but its collision, so the collision is drawn
    // whatever the collision toggle says. The toggle keeps the user's choice: both panes share it and it carries over
    // to the next file, so turning it on for one skeleton would change every other view.
    [[nodiscard]] bool DrawsCollision() const;
    [[nodiscard]] std::vector<const OpenGLShape*> DrawnShapes() const;
    void Cleanup();
    void CopySceneColorTexture(QOpenGLFunctions_2_1* a_f);
    void EnsureCollisionOverlay();
    void EnsureSceneColorTexture(QOpenGLFunctions_2_1* a_f);
    // Centers the scene and fits it in the view at these angles.
    void FrameCamera(const ViewAngles& a_angles);
    void FrameCameraIfNeeded();
    void ReleaseSceneColorTexture(QOpenGLFunctions_2_1* a_f);
    void RenderCollisionOverlay();
    void RenderRefractionProxyPass(QOpenGLFunctions_2_1* a_f);
    [[nodiscard]] ViewAngles SceneOpeningView() const;
    void SetProjectionMatrix();
    // Turns to an axis view, or to its opposite when the camera already shows it.
    void ShowAxisView(ViewAxis a_axis);
    void UpdateCamera();
    void UpdateSceneBounds();

    std::shared_ptr<nifly::NifFile> _nifFile;
    // It reads the file's keys, so it's declared after the file.
    Animation::SceneAnimation _animation;
    std::optional<float> _animationTime;
    bool _isGrassModel = false;

    // The texture manager reads through it, so it's declared first and outlives the manager.
    std::shared_ptr<const GameFiles> _gameFiles;
    std::unique_ptr<TextureManager> _textureManager;
    std::unique_ptr<ShaderManager> _shaderManager;

    std::vector<OpenGLShape> _glShapes;
    std::vector<OpenGLShape> _editorMarkerShapes;
    std::vector<SkippedShape> _skippedShapes;
    QVector<TextureResult> _textureResults;
    nifly::BoundingSphere _sceneBounds;
    BoundingBox _sceneBox;
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
    // Where the drag in progress has turned the view, before any snapping.
    ViewAngles _dragAngles;
    ViewButtons* _viewButtons = nullptr;
};
