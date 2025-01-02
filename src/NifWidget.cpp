#include "NifWidget.h"
#include "NifExtensions.h"

#include <QOpenGLContext>
#include <QOpenGLFunctions_2_1>
#include <QOpenGLVersionFunctionsFactory>
#include <QWheelEvent>
#include <utility>
using OpenGLFunctions = QOpenGLFunctions_2_1;

NifWidget::NifWidget(std::shared_ptr<nifly::NifFile> nifFile,
                     MOBase::IOrganizer* organizer, const bool debugContext,
                     QWidget* parent, const Qt::WindowFlags f)
  : QOpenGLWidget(parent, f), m_NifFile{std::move(nifFile)}, m_MOInfo{organizer},
    m_TextureManager{std::make_unique<TextureManager>(organizer)},
    m_ShaderManager{std::make_unique<ShaderManager>(organizer)}
{
  QSurfaceFormat format;
  format.setDepthBufferSize(24);
  format.setVersion(2, 1);
  format.setProfile(QSurfaceFormat::CoreProfile);

  if (debugContext) {
    format.setOption(QSurfaceFormat::DebugContext);
    m_Context = new QOpenGLContext();
    m_Context->setFormat(format);
    m_Context->create();
  }

  setFormat(format);
}

NifWidget::~NifWidget()
{
  cleanup();
}

void NifWidget::mousePressEvent(QMouseEvent* event)
{
  m_MousePos = event->globalPosition();
}

void NifWidget::mouseMoveEvent(QMouseEvent* event)
{
  const auto pos   = event->globalPosition();
  const auto delta = pos - m_MousePos;
  m_MousePos       = pos;

  switch (event->buttons()) {
  case Qt::LeftButton: {
    m_Camera->rotate(static_cast<float>(delta.x() * 0.5f),
                     static_cast<float>(delta.y() * 0.5f));
  }
  break;
  case Qt::MiddleButton: {
    const float viewDX = m_Camera->distance() / m_ViewportWidth;
    const float viewDY = m_Camera->distance() / m_ViewportHeight;

    QMatrix4x4 r;
    r.rotate(-m_Camera->yaw(), 0.0f, 1.0f, 0.0f);
    r.rotate(-m_Camera->pitch(), 1.0f, 0.0f, 0.0f);

    const auto pan = r * QVector4D(static_cast<float>(-delta.x() * viewDX),
                                   static_cast<float>(delta.y() * viewDY), 0.0f, 0.0f);

    m_Camera->pan(QVector3D(pan));
  }
  break;

  case Qt::RightButton: {
    if (event->modifiers() == Qt::ShiftModifier) {
      m_Camera->zoomDistance(static_cast<float>(delta.y() * 0.1f));
    }
  }
  break;
  default: ;
  }
}

void NifWidget::wheelEvent(QWheelEvent* event)
{
  m_Camera->zoomFactor(1.0f -
                       (static_cast<float>(event->angleDelta().y()) / 120.0f * 0.38f));
}

void NifWidget::messageLogged(const QOpenGLDebugMessage& message)
{
  const auto msg = tr("OpenGL debug message: %1").arg(message.message());
  qDebug(qUtf8Printable(msg));
}

void NifWidget::initializeGL()
{
  if (m_Context) {
    m_Logger = new QOpenGLDebugLogger(m_Context);
    if (m_Logger->initialize()) {
      m_Logger->enableMessages();
      qDebug() << "GL_DEBUG Debug Logger" << m_Logger;
      connect(m_Logger, &QOpenGLDebugLogger::messageLogged, this,
              &NifWidget::messageLogged);
      m_Logger->startLogging();
    }
  }

  auto shapes = m_NifFile->GetShapes();
  for (auto& shape : shapes) {
    if (shape->flags & TriShape::Hidden) {
      continue;
    }

    m_GLShapes.emplace_back(m_NifFile.get(), shape, m_TextureManager.get());
  }

  m_Camera = SharedCamera;
  if (m_Camera.isNull()) {
    m_Camera     = {new Camera(), &Camera::deleteLater};
    SharedCamera = m_Camera;

    float largestRadius = 0.0f;
    for (const auto& shape : shapes) {

      if (auto bounds = GetBoundingSphere(m_NifFile.get(), shape);
        bounds.radius > largestRadius) {
        largestRadius = bounds.radius;

        m_Camera->setDistance(bounds.radius * 2.4f);
        m_Camera->setLookAt({-bounds.center.x, bounds.center.z, bounds.center.y});
      }
    }
  }

  updateCamera();

  connect(m_Camera.get(), &Camera::cameraMoved, this, [this]() {
    updateCamera();
    update();
  });

  const auto f = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_2_1>(
      QOpenGLContext::currentContext());

  f->glEnable(GL_DEPTH_TEST);
  f->glDepthFunc(GL_LEQUAL);
  f->glClearColor(0.18, 0.18, 0.18, 1.0);
}

void NifWidget::paintGL()
{
  const auto f = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_2_1>(
      QOpenGLContext::currentContext());
  f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  std::vector<OpenGLShape*> opaqueShapes;
  std::vector<OpenGLShape*> transparentShapes;

  for (auto& shape : m_GLShapes) {
    if (shape.alpha < 1.0f || shape.alphaBlendEnable) {
      transparentShapes.push_back(&shape);
    } else {
      opaqueShapes.push_back(&shape);
    }
  }

  for (const auto* shape : opaqueShapes) {
    if (const auto program = m_ShaderManager->getProgram(shape->shaderType);
      program && program->isLinked() && program->bind()) {
      auto binder = QOpenGLVertexArrayObject::Binder(shape->vertexArray);

      auto& modelMatrix    = shape->modelMatrix;
      auto modelViewMatrix = m_ViewMatrix * modelMatrix;
      auto mvpMatrix       = m_ProjectionMatrix * modelViewMatrix;

      program->setUniformValue("worldMatrix", modelMatrix);
      program->setUniformValue("viewMatrix", m_ViewMatrix);
      program->setUniformValue("modelViewMatrix", modelViewMatrix);
      program->setUniformValue("modelViewMatrixInverse", modelViewMatrix.inverted());
      program->setUniformValue("normalMatrix", modelViewMatrix.normalMatrix());
      program->setUniformValue("mvpMatrix", mvpMatrix);
      program->setUniformValue("lightDirection", QVector3D(0, 0, 1));

      shape->setupShaders(program);

      if (shape->indexBuffer && shape->indexBuffer->isCreated()) {
        shape->indexBuffer->bind();
        f->glDrawElements(GL_TRIANGLES, shape->elements, GL_UNSIGNED_SHORT, nullptr);
        shape->indexBuffer->release();
      }

      program->release();
    }
  }

  f->glEnable(GL_BLEND);
  f->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  f->glDepthMask(GL_FALSE);

  for (const auto* shape : transparentShapes) {
    if (const auto program = m_ShaderManager->getProgram(shape->shaderType);
      program && program->isLinked() && program->bind()) {
      auto binder = QOpenGLVertexArrayObject::Binder(shape->vertexArray);

      auto& modelMatrix    = shape->modelMatrix;
      auto modelViewMatrix = m_ViewMatrix * modelMatrix;
      auto mvpMatrix       = m_ProjectionMatrix * modelViewMatrix;

      program->setUniformValue("worldMatrix", modelMatrix);
      program->setUniformValue("viewMatrix", m_ViewMatrix);
      program->setUniformValue("modelViewMatrix", modelViewMatrix);
      program->setUniformValue("modelViewMatrixInverse", modelViewMatrix.inverted());
      program->setUniformValue("normalMatrix", modelViewMatrix.normalMatrix());
      program->setUniformValue("mvpMatrix", mvpMatrix);
      program->setUniformValue("lightDirection", QVector3D(0, 0, 1));

      shape->setupShaders(program);

      if (shape->indexBuffer && shape->indexBuffer->isCreated()) {
        shape->indexBuffer->bind();
        f->glDrawElements(GL_TRIANGLES, shape->elements, GL_UNSIGNED_SHORT, nullptr);
        shape->indexBuffer->release();
      }

      program->release();
    }
  }

  f->glDepthMask(GL_TRUE);
  f->glDisable(GL_BLEND);
}

void NifWidget::resizeGL(const int w, const int h)
{
  QMatrix4x4 m;
  m.perspective(40.0f, static_cast<float>(w) / static_cast<float>(h), 10.0f, 10000.0f);

  m_ProjectionMatrix = m;
  m_ViewportWidth    = static_cast<float>(w);
  m_ViewportHeight   = static_cast<float>(h);
}

void NifWidget::cleanup()
{
  makeCurrent();

  for (auto& shape : m_GLShapes) {
    shape.destroy();
  }
  m_GLShapes.clear();

  m_TextureManager->cleanup();
}

void NifWidget::updateCamera()
{
  QMatrix4x4 m;
  m.translate(0.0f, 0.0f, -m_Camera->distance());
  m.rotate(m_Camera->pitch(), 1.0f, 0.0f, 0.0f);
  m.rotate(m_Camera->yaw(), 0.0f, 1.0f, 0.0f);
  m.translate(-m_Camera->lookAt());
  m *= QMatrix4x4{
      -1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1,
  };
  m_ViewMatrix = m;
}
