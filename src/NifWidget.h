#pragma once

#include "Camera.h"
#include "OpenGLShape.h"
#include "ShaderManager.h"
#include "TextureManager.h"
#include "TextureSource.h"

#include <QOpenGLDebugLogger>
#include <QOpenGLFunctions_2_1>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
#include <QSharedPointer>

#include <NifFile.hpp>
#include <uibase/imoinfo.h>

#include <memory>

class OpenGLCollisionOverlay;

class NifWidget final : public QOpenGLWidget
{
  Q_OBJECT

public:
  NifWidget(std::shared_ptr<nifly::NifFile> nifFile, MOBase::IOrganizer* organizer,
            QSharedPointer<Camera> camera       = {},
            TextureSourceProvider textureSource = {}, bool debugContext = false,
            QWidget* parent = nullptr, Qt::WindowFlags f = {0});

  ~NifWidget() override;
  NifWidget(const NifWidget&)            = delete;
  NifWidget(NifWidget&&)                 = delete;
  NifWidget& operator=(const NifWidget&) = delete;
  NifWidget& operator=(NifWidget&&)      = delete;

  [[nodiscard]] QSharedPointer<Camera> camera() const { return m_Camera; }
  void setCamera(QSharedPointer<Camera> camera);
  void setShowCollision(bool showCollision);
  void resetCamera();

protected:
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;

  void initializeGL() override;
  void paintGL() override;
  void resizeGL(int w, int h) override;

private:
  void cleanup();
  void copySceneColorTexture(QOpenGLFunctions_2_1* f);
  void ensureCollisionOverlay();
  void ensureSceneColorTexture(QOpenGLFunctions_2_1* f);
  void frameCameraIfNeeded();
  void releaseSceneColorTexture(QOpenGLFunctions_2_1* f);
  void renderCollisionOverlay();
  void renderRefractionProxyPass(QOpenGLFunctions_2_1* f);
  void setProjectionMatrix();
  void updateCamera();

  std::shared_ptr<nifly::NifFile> m_NifFile;
  MOBase::IOrganizer* m_MOInfo = nullptr;

  std::unique_ptr<TextureManager> m_TextureManager;
  std::unique_ptr<ShaderManager> m_ShaderManager;

  QOpenGLDebugLogger* m_Logger = nullptr;
  QOpenGLContext* m_Context    = nullptr;

  std::vector<OpenGLShape> m_GLShapes;
  std::unique_ptr<OpenGLCollisionOverlay> m_CollisionOverlay;
  bool m_CollisionOverlayBuildAttempted = false;
  bool m_ShowCollision                  = false;

  QSharedPointer<Camera> m_Camera;
  QMetaObject::Connection m_CameraConnection;

  QMatrix4x4 m_ViewMatrix;
  QMatrix4x4 m_ProjectionMatrix;

  GLuint m_SceneColorTexture    = 0;
  int m_SceneColorTextureWidth  = 0;
  int m_SceneColorTextureHeight = 0;

  float m_ViewportWidth{};
  float m_ViewportHeight{};
  QPointF m_MousePos;

  static void messageLogged(const QOpenGLDebugMessage& message);
};
