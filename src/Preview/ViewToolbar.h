#pragma once

#include <QWidget>

class QCheckBox;
class QPushButton;

// The controls both panes share.
class ViewToolbar final : public QWidget {
    Q_OBJECT

public:
    explicit ViewToolbar(QWidget* a_parent = nullptr);

    void SetShowTextures(bool a_show);
    void SetSplit(bool a_enabled);
    // Disables Split Preview and gives the reason as its tooltip. Empty when it can be used.
    void SetSplitUnavailableReason(const QString& a_reason);
    // A NIF without meshes always draws its collision, so the option shows as on and can't be changed.
    void SetCollisionLocked(bool a_locked);
    [[nodiscard]] bool SyncsCameras() const;

signals:
    void ResetCameraClicked();
    void CollisionToggled(bool a_show);
    void EditorMarkersToggled(bool a_show);
    void TexturesToggled(bool a_show);
    void SplitToggled(bool a_enabled);
    void CameraSyncToggled(bool a_enabled);

private:
    void UpdateCollisionBox();
    void UpdateSplitControls();

    QPushButton* _resetCameraButton = nullptr;
    QCheckBox* _collisionBox = nullptr;
    QCheckBox* _editorMarkersBox = nullptr;
    QCheckBox* _texturesBox = nullptr;
    QCheckBox* _splitBox = nullptr;
    QCheckBox* _cameraSyncBox = nullptr;
    bool _showCollision = false;
    bool _collisionLocked = false;
};
