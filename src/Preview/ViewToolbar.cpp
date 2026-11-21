#include "Preview/ViewToolbar.h"
#include "Preview/FlowLayout.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>

namespace {
constexpr int kHorizontalSpacing = 12;
constexpr int kVerticalSpacing = 4;
} // namespace

ViewToolbar::ViewToolbar(QWidget* a_parent)
    : QWidget(a_parent) {
    _resetCameraButton = new QPushButton(tr("Reset Camera"), this);
    // The preview sits in a dialog, where Enter would otherwise press the button.
    _resetCameraButton->setAutoDefault(false);
    _collisionBox = new QCheckBox(tr("Show Collision"), this);
    _editorMarkersBox = new QCheckBox(tr("Show Editor Markers"), this);
    _editorMarkersBox->setToolTip(tr("Show the Creation Kit markers the game hides"));
    _texturesBox = new QCheckBox(tr("Show Textures"), this);
    _texturesBox->setToolTip(tr("List the textures and the mod each one comes from"));
    _animationBox = new QCheckBox(tr("Play Animation"), this);
    _animationBox->setToolTip(tr("Play the mesh's own animation"));
    _sequenceRow = new QWidget(this);
    _sequenceBox = new QComboBox(_sequenceRow);
    _sequenceBox->setToolTip(tr("The animation that plays, of the ones this mesh has"));
    _sequenceBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    auto* const sequenceLabel = new QLabel(tr("Animation:"), _sequenceRow);
    sequenceLabel->setBuddy(_sequenceBox);
    auto* const sequenceLayout = new QHBoxLayout(_sequenceRow);
    sequenceLayout->setContentsMargins(0, 0, 0, 0);
    sequenceLayout->addWidget(sequenceLabel);
    sequenceLayout->addWidget(_sequenceBox);
    _splitBox = new QCheckBox(tr("Split Preview"), this);
    _cameraSyncBox = new QCheckBox(tr("Sync Cameras"), this);
    _cameraSyncBox->setToolTip(tr("Move both cameras together"));
    _cameraSyncBox->setChecked(true);

    auto* const layout = new FlowLayout(this, kHorizontalSpacing, kVerticalSpacing);
    layout->addWidget(_resetCameraButton);
    layout->addWidget(_collisionBox);
    layout->addWidget(_editorMarkersBox);
    layout->addWidget(_texturesBox);
    layout->addWidget(_animationBox);
    layout->addWidget(_sequenceRow);
    layout->addWidget(_splitBox);
    layout->addWidget(_cameraSyncBox);

    connect(_resetCameraButton, &QPushButton::clicked, this, &ViewToolbar::ResetCameraClicked);
    connect(_collisionBox, &QCheckBox::toggled, this, [this](const bool a_show) {
        _showCollision = a_show;
        emit CollisionToggled(a_show);
    });
    connect(_editorMarkersBox, &QCheckBox::toggled, this, &ViewToolbar::EditorMarkersToggled);
    connect(_texturesBox, &QCheckBox::toggled, this, &ViewToolbar::TexturesToggled);
    connect(_animationBox, &QCheckBox::toggled, this, &ViewToolbar::AnimationToggled);
    connect(_sequenceBox, &QComboBox::currentTextChanged, this, &ViewToolbar::SequenceChosen);
    connect(_splitBox, &QCheckBox::toggled, this, [this](const bool a_enabled) {
        UpdateSplitControls();
        emit SplitToggled(a_enabled);
    });
    connect(_cameraSyncBox, &QCheckBox::toggled, this, &ViewToolbar::CameraSyncToggled);

    UpdateCollisionBox();
    UpdateSplitControls();
    SetSplitUnavailableReason({});
    SetAnimationAvailable(false);
    SetSequences({}, {});
}

void ViewToolbar::SetPlaysAnimation(const bool a_plays) {
    const QSignalBlocker blocker(_animationBox);
    _animationBox->setChecked(a_plays);
}

void ViewToolbar::SetAnimationAvailable(const bool a_available) {
    _animationBox->setVisible(a_available);
}

void ViewToolbar::SetSequences(const QStringList& a_names, const QString& a_playing) {
    const QSignalBlocker blocker(_sequenceBox);
    // Choosing an animation ends up here while the list is still handling the choice, so it's only rebuilt for
    // other names.
    if (a_names != _sequenceNames) {
        _sequenceNames = a_names;
        _sequenceBox->clear();
        _sequenceBox->addItems(a_names);
    }
    _sequenceBox->setCurrentText(a_playing);
    _sequenceRow->setVisible(a_names.size() > 1);
}

bool ViewToolbar::PlaysAnimation() const {
    return _animationBox->isChecked();
}

void ViewToolbar::SetShowCollision(const bool a_show) {
    _showCollision = a_show;
    UpdateCollisionBox();
}

void ViewToolbar::SetShowEditorMarkers(const bool a_show) {
    const QSignalBlocker blocker(_editorMarkersBox);
    _editorMarkersBox->setChecked(a_show);
}

void ViewToolbar::SetShowTextures(const bool a_show) {
    const QSignalBlocker blocker(_texturesBox);
    _texturesBox->setChecked(a_show);
}

void ViewToolbar::SetSplit(const bool a_enabled) {
    const QSignalBlocker blocker(_splitBox);
    _splitBox->setChecked(a_enabled);
    UpdateSplitControls();
}

void ViewToolbar::SetSplitUnavailableReason(const QString& a_reason) {
    _splitBox->setEnabled(a_reason.isEmpty());
    _splitBox->setToolTip(a_reason.isEmpty() ? tr("Show two copies of this mesh side by side") : a_reason);
}

void ViewToolbar::SetCollisionLocked(const bool a_locked) {
    _collisionLocked = a_locked;
    UpdateCollisionBox();
}

bool ViewToolbar::SyncsCameras() const {
    return _cameraSyncBox->isChecked();
}

void ViewToolbar::UpdateCollisionBox() {
    const QSignalBlocker blocker(_collisionBox);
    _collisionBox->setChecked(_showCollision || _collisionLocked);
    _collisionBox->setEnabled(!_collisionLocked);
    _collisionBox->setToolTip(
        _collisionLocked ? tr("This NIF has no meshes, so its collision always shows") : tr("Show the collision shapes")
    );
}

void ViewToolbar::UpdateSplitControls() {
    const auto split = _splitBox->isChecked();
    _cameraSyncBox->setVisible(split);
    _resetCameraButton->setToolTip(split ? tr("Reset both preview cameras") : tr("Reset the preview camera"));
}
