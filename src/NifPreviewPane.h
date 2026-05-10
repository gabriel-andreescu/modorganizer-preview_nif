#pragma once

#include "Camera.h"
#include "NifPreviewSource.h"
#include "TextureSource.h"

#include <QSharedPointer>
#include <QWidget>

#include <memory>

class QComboBox;
class QLabel;
class QResizeEvent;
class QToolButton;
class QVBoxLayout;
class NifWidget;

namespace MOBase
{
class IOrganizer;
}

class NifPreviewPane final : public QWidget
{
  Q_OBJECT

public:
  explicit NifPreviewPane(MOBase::IOrganizer* organizer, QWidget* parent = nullptr);

  void setProviders(QVector<NifPreviewProvider> providers, int currentIndex);
  void setCamera(QSharedPointer<Camera> camera);
  void setShowCollision(bool showCollision);
  void resetCamera();
  [[nodiscard]] QSharedPointer<Camera> camera() const { return m_Camera; }
  [[nodiscard]] int currentProviderIndex() const { return m_CurrentProviderIndex; }

signals:
  void providerChanged(int providerIndex);
  void cameraMoved();

protected:
  void resizeEvent(QResizeEvent* event) override;

private:
  void selectProvider(int index);
  void selectRelativeProvider(int offset);
  void selectTextureSource(int index);
  void selectRelativeTextureSource(int offset);
  void updateControls();
  void updateSourceComboWidth();
  void setTextureSources(TextureSourceSet sourceSet);
  void updateTextureControls();
  void updateTextureSourceComboWidth();
  void loadCurrentProvider();
  void reloadCurrentNifWidget();
  void setViewWidget(QWidget* widget);
  [[nodiscard]] TextureSourceProvider currentTextureSourceProvider() const;

  MOBase::IOrganizer* m_Organizer = nullptr;
  QVector<NifPreviewProvider> m_Providers;
  int m_CurrentProviderIndex = 0;
  bool m_UpdatingControls    = false;
  TextureSourceSet m_TextureSourceSet;
  int m_CurrentTextureSourceIndex = 0;
  bool m_UpdatingTextureControls  = false;
  bool m_ShowCollision            = false;

  QSharedPointer<Camera> m_Camera;
  QMetaObject::Connection m_CameraConnection;
  QLabel* m_TitleLabel             = nullptr;
  QToolButton* m_PrevButton        = nullptr;
  QComboBox* m_SourceCombo         = nullptr;
  QToolButton* m_NextButton        = nullptr;
  QLabel* m_TextureLabel           = nullptr;
  QToolButton* m_PrevTextureButton = nullptr;
  QComboBox* m_TextureSourceCombo  = nullptr;
  QToolButton* m_NextTextureButton = nullptr;
  QLabel* m_StatsLabel             = nullptr;
  QVBoxLayout* m_ViewLayout        = nullptr;
  QWidget* m_ViewWidget            = nullptr;
  NifWidget* m_NifWidget           = nullptr;
  std::shared_ptr<nifly::NifFile> m_CurrentNifFile;
};
