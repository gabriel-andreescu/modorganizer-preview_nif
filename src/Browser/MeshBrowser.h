#pragma once

#include "Preview/Preferences.h"

#include <QDialog>
#include <QPointer>
#include <QTimer>

class MeshCatalog;
class MeshFilterModel;
class MeshTreeModel;
class NifPreviewWidget;
class PreviewNif;
class QLabel;
class QLineEdit;
class QProgressBar;
class QSplitter;
class QTreeView;
class QVBoxLayout;

namespace MOBase {
class IOrganizer;
}

// Lists all meshes, loose and in the loaded archives, and previews the selected one.
class MeshBrowser final : public QDialog {
    Q_OBJECT

public:
    MeshBrowser(
        const PreviewNif& a_plugin,
        MeshCatalog& a_catalog,
        const MOBase::IOrganizer& a_organizer,
        QWidget* a_parent = nullptr
    );

    void SetFilterText(const QString& a_text);
    // Selects the mesh and previews it at once. False when the list has no such mesh.
    bool ShowMesh(const QString& a_dataPath);

    [[nodiscard]] bool IsReady() const;
    [[nodiscard]] qsizetype MeshCount() const;
    [[nodiscard]] QString FilterText() const;
    // The data path of the previewed mesh. Empty before the first one.
    [[nodiscard]] const QString& ShownPath() const noexcept {
        return _shownPath;
    }
    // Null before the first mesh and for a mesh that can't be previewed.
    [[nodiscard]] NifPreviewWidget* Preview() const;

    void done(int a_result) override;

private:
    void ConnectControls();
    void ShowProgress(int a_archivesRead, int a_archives);
    void AddMeshes(const QStringList& a_dataPaths);
    void FinishLoading();
    void ApplyFilter();
    void ExpandFiltered();
    void HandleCurrentChanged(const QModelIndex& a_current);
    void PreviewPending();
    void ShowPreview(const QString& a_dataPath);
    void SetPreviewWidget(QWidget* a_widget);
    void RestoreLayout();

    const PreviewNif* _plugin;
    MeshCatalog* _catalog;
    PreviewPreferences _preferences;

    QLineEdit* _filterEdit = nullptr;
    QTreeView* _tree = nullptr;
    QProgressBar* _progress = nullptr;
    QLabel* _statusLabel = nullptr;
    QSplitter* _splitter = nullptr;
    QVBoxLayout* _previewLayout = nullptr;
    QWidget* _previewWidget = nullptr;
    QPointer<NifPreviewWidget> _preview;
    MeshTreeModel* _model = nullptr;
    MeshFilterModel* _filter = nullptr;

    // Typing and moving through the list each wait a moment, so only the text or mesh the user stops on is acted on.
    QTimer _filterTimer;
    QTimer _previewTimer;
    QString _pendingPath;
    QString _shownPath;
    bool _openedTopLevel = false;
};
