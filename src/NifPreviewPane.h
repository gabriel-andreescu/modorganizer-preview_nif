#pragma once

#include "Camera.h"
#include "PreviewPaneController.h"

#include <QSharedPointer>
#include <QWidget>

class QComboBox;
class QLabel;
class QResizeEvent;
class QToolButton;
class QVBoxLayout;
class NifWidget;

namespace MOBase {
class IOrganizer;
}

class NifPreviewPane final : public QWidget {
    Q_OBJECT

public:
    explicit NifPreviewPane(MOBase::IOrganizer* organizer, QWidget* parent = nullptr);

    void setProviders(QVector<NifPreviewProvider> providers, int currentIndex);
    void setCamera(QSharedPointer<Camera> camera);
    void setShowCollision(bool showCollision);
    void resetCamera();
    [[nodiscard]] QSharedPointer<Camera> camera() const {
        return m_Camera;
    }
    [[nodiscard]] int currentProviderIndex() const {
        return m_Controller.currentProviderIndex();
    }

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
    void updateTextureSourceComboItems();
    void updateTextureControls();
    void updateTextureSourceComboWidth();
    void loadCurrentProvider();
    void reloadCurrentNifWidget();
    void setViewWidget(QWidget* widget);

    MOBase::IOrganizer* m_Organizer = nullptr;
    PreviewPaneController m_Controller;
    bool m_UpdatingControls = false;
    bool m_UpdatingTextureControls = false;
    bool m_ShowCollision = false;

    QSharedPointer<Camera> m_Camera;
    QMetaObject::Connection m_CameraConnection;
    QLabel* m_TitleLabel = nullptr;
    QToolButton* m_PrevButton = nullptr;
    QComboBox* m_SourceCombo = nullptr;
    QToolButton* m_NextButton = nullptr;
    QLabel* m_TextureLabel = nullptr;
    QToolButton* m_PrevTextureButton = nullptr;
    QComboBox* m_TextureSourceCombo = nullptr;
    QToolButton* m_NextTextureButton = nullptr;
    QLabel* m_StatsLabel = nullptr;
    QVBoxLayout* m_ViewLayout = nullptr;
    QWidget* m_ViewWidget = nullptr;
    NifWidget* m_NifWidget = nullptr;
};
