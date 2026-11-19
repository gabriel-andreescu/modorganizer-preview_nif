#pragma once

#include "Preview/Source.h"

#include <QWidget>

class QComboBox;
class QPushButton;

// Picks which copy of the NIF the pane shows, highest priority first.
class VersionPicker final : public QWidget {
    Q_OBJECT

public:
    explicit VersionPicker(QWidget* a_parent = nullptr);

    void SetVersions(const QVector<NifPreviewProvider>& a_providers, int a_current);

signals:
    void VersionSelected(int a_index);
    void HideOthersClicked(int a_index);
    void UnhideClicked(int a_index);

private:
    void UpdateActions();

    QVector<NifPreviewProvider> _providers;
    QComboBox* _combo = nullptr;
    QPushButton* _hideOthersButton = nullptr;
    QPushButton* _unhideButton = nullptr;
};
