#pragma once

#include "Camera.h"
#include "NifPreviewSource.h"

#include <QSharedPointer>
#include <QWidget>

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
  void updateControls();
  void updateSourceComboWidth();
  void loadCurrentProvider();
  void setViewWidget(QWidget* widget);

  MOBase::IOrganizer* m_Organizer = nullptr;
  QVector<NifPreviewProvider> m_Providers;
  int m_CurrentProviderIndex = 0;
  bool m_UpdatingControls    = false;

  QSharedPointer<Camera> m_Camera;
  QMetaObject::Connection m_CameraConnection;
  QLabel* m_TitleLabel      = nullptr;
  QToolButton* m_PrevButton = nullptr;
  QComboBox* m_SourceCombo  = nullptr;
  QToolButton* m_NextButton = nullptr;
  QLabel* m_StatsLabel      = nullptr;
  QVBoxLayout* m_ViewLayout = nullptr;
  QWidget* m_ViewWidget     = nullptr;
  NifWidget* m_NifWidget    = nullptr;
};
