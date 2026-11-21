#include "Browser/MeshBrowser.h"
#include "Browser/MeshFilterModel.h"
#include "Browser/MeshTreeModel.h"
#include "Browser/ScrollAnchor.h"
#include "Data/GameFiles.h"
#include "Data/MeshCatalog.h"
#include "Preview/CopyText.h"
#include "Preview/Widget.h"
#include "PreviewNif.h"
#include "Render/Camera.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QOpenGLWidget>
#include <QProgressBar>
#include <QSplitter>
#include <QTreeView>
#include <QVBoxLayout>

#include <optional>

namespace {
constexpr QSize kDefaultSize {1200, 760};
constexpr int kListWidth = 340;
constexpr int kNameColumnWidth = 210;
constexpr int kFilterDelayMs = 250;
constexpr int kPreviewDelayMs = 150;
// Shorter filter text matches most of the list, and expanding all of it takes long.
constexpr qsizetype kExpandFilterLength = 3;

QLabel* MakeMessage(const QString& a_text, QWidget* a_parent) {
    auto* const label = new QLabel(a_text, a_parent);
    label->setAlignment(Qt::AlignCenter);
    return label;
}
} // namespace

MeshBrowser::MeshBrowser(
    const PreviewNif& a_plugin,
    MeshCatalog& a_catalog,
    const MOBase::IOrganizer& a_organizer,
    QWidget* a_parent
)
    : QDialog(a_parent)
    , _plugin(&a_plugin)
    , _catalog(&a_catalog)
    , _preferences(a_organizer) {
    setWindowTitle(tr("NIF Browser"));
    setAttribute(Qt::WA_DeleteOnClose);

    _filterEdit = new QLineEdit(this);
    _filterEdit->setPlaceholderText(tr("Filter"));
    _filterEdit->setClearButtonEnabled(true);

    _model = new MeshTreeModel(this);
    _filter = new MeshFilterModel(this);
    _filter->setSourceModel(_model);
    _tree = new QTreeView(this);
    _tree->setModel(_filter);
    _tree->setUniformRowHeights(true);
    _tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _tree->header()->setStretchLastSection(true);
    _tree->setColumnWidth(0, kNameColumnWidth);
    new ScrollAnchor(*_tree);

    _statusLabel = new QLabel(this);
    _progress = new QProgressBar(this);
    _progress->setTextVisible(false);
    // As tall as the text beside it, so the list keeps its height when the bar goes away.
    _progress->setFixedHeight(fontMetrics().height());
    auto* const status = new QHBoxLayout();
    status->addWidget(_statusLabel);
    status->addWidget(_progress, 1);

    auto* const list = new QWidget(this);
    auto* const listLayout = new QVBoxLayout(list);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->addWidget(_filterEdit);
    listLayout->addWidget(_tree, 1);
    listLayout->addLayout(status);

    auto* const preview = new QWidget(this);
    _previewLayout = new QVBoxLayout(preview);
    _previewLayout->setContentsMargins(0, 0, 0, 0);
    SetPreviewWidget(MakeMessage(tr("Select a mesh to preview it"), preview));

    _splitter = new QSplitter(Qt::Horizontal, this);
    _splitter->addWidget(list);
    _splitter->addWidget(preview);
    _splitter->setStretchFactor(0, 0);
    _splitter->setStretchFactor(1, 1);
    _splitter->setChildrenCollapsible(false);
    _splitter->setSizes({kListWidth, kDefaultSize.width() - kListWidth});

    auto* const layout = new QVBoxLayout(this);
    layout->addWidget(_splitter);

    // Qt recreates a shown window when it gets its first OpenGL widget, and the window blinks. Holding one from the
    // start creates the window ready for the previews.
    auto* const openGlHolder = new QOpenGLWidget(this);
    openGlHolder->hide();

    _filterTimer.setSingleShot(true);
    _filterTimer.setInterval(kFilterDelayMs);
    _previewTimer.setSingleShot(true);
    _previewTimer.setInterval(kPreviewDelayMs);

    ConnectControls();
    RestoreLayout();
    _catalog->Refresh();
}

void MeshBrowser::ConnectControls() {
    connect(_catalog, &MeshCatalog::Found, this, &MeshBrowser::AddMeshes);
    connect(_catalog, &MeshCatalog::Progress, this, &MeshBrowser::ShowProgress);
    connect(_catalog, &MeshCatalog::Ready, this, &MeshBrowser::FinishLoading);
    connect(_filterEdit, &QLineEdit::textChanged, &_filterTimer, qOverload<>(&QTimer::start));
    connect(&_filterTimer, &QTimer::timeout, this, &MeshBrowser::ApplyFilter);
    connect(_tree->selectionModel(), &QItemSelectionModel::currentChanged, this, &MeshBrowser::HandleCurrentChanged);
    connect(_tree, &QTreeView::activated, this, [this](const QModelIndex& a_index) {
        HandleCurrentChanged(a_index);
        PreviewPending();
    });
    connect(&_previewTimer, &QTimer::timeout, this, &MeshBrowser::PreviewPending);
}

void MeshBrowser::SetFilterText(const QString& a_text) {
    _filterEdit->setText(a_text);
    ApplyFilter();
}

bool MeshBrowser::ShowMesh(const QString& a_dataPath) {
    const auto index = _filter->mapFromSource(_model->IndexOf(a_dataPath));
    if (!index.isValid()) {
        return false;
    }

    _tree->setCurrentIndex(index);
    _tree->scrollTo(index);
    PreviewPending();
    return true;
}

bool MeshBrowser::IsReady() const {
    return _catalog->IsReady();
}

qsizetype MeshBrowser::MeshCount() const {
    return _model->MeshCount();
}

NifPreviewWidget* MeshBrowser::Preview() const {
    return _preview;
}

QString MeshBrowser::FilterText() const {
    return _filterEdit->text();
}

void MeshBrowser::done(const int a_result) {
    _preferences.SetBrowserGeometry(saveGeometry());
    _preferences.SetBrowserSplitter(_splitter->saveState());
    QDialog::done(a_result);
}

void MeshBrowser::AddMeshes(const QStringList& a_dataPaths) {
    _model->AddMeshes(a_dataPaths);

    // The data folder's meshes directory is the only top-level entry for most games, so it opens by itself.
    if (!_openedTopLevel && _filter->rowCount() == 1) {
        _openedTopLevel = true;
        _tree->expand(_filter->index(0, 0));
    }
    ExpandFiltered();
}

void MeshBrowser::ShowProgress(const int a_archivesRead, const int a_archives) {
    _statusLabel->setText(tr("Reading archives: %1 of %2").arg(a_archivesRead).arg(a_archives));
    _progress->setRange(0, a_archives);
    _progress->setValue(a_archivesRead);
}

// The Mod column waits for every archive, since naming a mesh's winner takes the member list of each of them.
void MeshBrowser::FinishLoading() {
    _model->SetWinnerLabel([gameFiles = _plugin->SnapshotGameFiles()](const QString& a_dataPath) {
        const auto copies = gameFiles->Sources(a_dataPath);
        return copies.isEmpty() ? QString() : CopyText::Label(copies, 0);
    });
    _progress->hide();
    _statusLabel->setText(tr("%1 meshes").arg(QLocale().toString(_model->MeshCount())));
}

void MeshBrowser::ApplyFilter() {
    _filterTimer.stop();
    _filter->SetFilterText(_filterEdit->text());
    ExpandFiltered();
}

void MeshBrowser::ExpandFiltered() {
    if (_filterEdit->text().trimmed().size() >= kExpandFilterLength) {
        _tree->expandAll();
    }
}

void MeshBrowser::HandleCurrentChanged(const QModelIndex& a_current) {
    const auto path = _model->DataPath(_filter->mapToSource(a_current));
    if (path.isEmpty()) {
        return;
    }

    _pendingPath = path;
    _previewTimer.start();
}

void MeshBrowser::PreviewPending() {
    _previewTimer.stop();
    if (!_pendingPath.isEmpty()) {
        ShowPreview(_pendingPath);
    }
}

void MeshBrowser::ShowPreview(const QString& a_dataPath) {
    if (a_dataPath.compare(_shownPath, Qt::CaseInsensitive) == 0) {
        return;
    }

    const auto options = _preview ? std::optional(_preview->Options()) : std::nullopt;
    _shownPath = a_dataPath;
    // Each mesh gets a camera of its own, so the view frames it instead of keeping the previous mesh's framing.
    _preview = _plugin->CreatePreview(a_dataPath, {}, {new Camera(), &Camera::deleteLater});
    if (!_preview) {
        SetPreviewWidget(MakeMessage(tr("This mesh can't be previewed"), this));
        return;
    }

    if (options) {
        _preview->SetOptions(*options);
    }
    SetPreviewWidget(_preview);
}

void MeshBrowser::SetPreviewWidget(QWidget* a_widget) {
    if (_previewWidget) {
        _previewLayout->removeWidget(_previewWidget);
        _previewWidget->deleteLater();
    }
    _previewWidget = a_widget;
    _previewLayout->addWidget(a_widget);
}

void MeshBrowser::RestoreLayout() {
    resize(kDefaultSize);
    if (const auto geometry = _preferences.BrowserGeometry(); !geometry.isEmpty()) {
        restoreGeometry(geometry);
    }
    if (const auto splitter = _preferences.BrowserSplitter(); !splitter.isEmpty()) {
        _splitter->restoreState(splitter);
    }
}
