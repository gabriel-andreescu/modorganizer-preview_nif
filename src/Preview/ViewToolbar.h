#pragma once

#include <QStringList>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QPushButton;

// The controls both panes share.
class ViewToolbar final : public QWidget {
    Q_OBJECT

public:
    explicit ViewToolbar(QWidget* a_parent = nullptr);

    void SetShowCollision(bool a_show);
    void SetShowEditorMarkers(bool a_show);
    void SetShowTextures(bool a_show);
    void SetSplit(bool a_enabled);
    // Disables Split Preview and gives the reason as its tooltip. Empty when it can be used.
    void SetSplitUnavailableReason(const QString& a_reason);
    // A NIF without meshes always draws its collision, so the option shows as on and can't be changed.
    void SetCollisionLocked(bool a_locked);
    void SetPlaysAnimation(bool a_plays);
    // Shows Play Animation only for a mesh that has animation.
    void SetAnimationAvailable(bool a_available);
    // Lists the mesh's named animations. The list is hidden when the mesh has fewer than two.
    void SetSequences(const QStringList& a_names, const QString& a_playing);
    [[nodiscard]] bool PlaysAnimation() const;
    [[nodiscard]] bool SyncsCameras() const;

signals:
    void ResetCameraClicked();
    void CollisionToggled(bool a_show);
    void EditorMarkersToggled(bool a_show);
    void TexturesToggled(bool a_show);
    void AnimationToggled(bool a_plays);
    void SequenceChosen(const QString& a_name);
    void SplitToggled(bool a_enabled);
    void CameraSyncToggled(bool a_enabled);

private:
    void UpdateCollisionBox();
    void UpdateSplitControls();

    QPushButton* _resetCameraButton = nullptr;
    QCheckBox* _collisionBox = nullptr;
    QCheckBox* _editorMarkersBox = nullptr;
    QCheckBox* _texturesBox = nullptr;
    QCheckBox* _animationBox = nullptr;
    QWidget* _sequenceRow = nullptr;
    QComboBox* _sequenceBox = nullptr;
    QStringList _sequenceNames;
    QCheckBox* _splitBox = nullptr;
    QCheckBox* _cameraSyncBox = nullptr;
    bool _showCollision = false;
    bool _collisionLocked = false;
};
