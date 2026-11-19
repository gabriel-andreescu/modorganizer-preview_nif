#include "PreviewNif.h"
#include "Browser/MeshBrowser.h"
#include "Data/ArchiveIndex.h"
#include "Data/GameFiles.h"
#include "Data/MeshCatalog.h"
#include "Data/MeshFiles.h"
#include "DevBench/Tool.h"
#include "Preview/Source.h"
#include "Preview/Widget.h"
#include "Render/Camera.h"

#include <DevBenchAPI.h>

#include <QDebug>
#include <QMainWindow>
#include <QTimer>
#include <algorithm>
#include <utility>

PreviewNif::PreviewNif()
    : _archiveIndex(std::make_unique<ArchiveIndex>()) {
    _instance = this;
}

PreviewNif::~PreviewNif() {
    _instance = nullptr;
}

bool PreviewNif::init(MOBase::IOrganizer* a_moInfo) {
    _moInfo = a_moInfo;
    _moInfo->onUserInterfaceInitialized([this](QMainWindow* a_mainWindow) {
        // Dev Bench starts from this notification too, so request its interface after every handler has run.
        QTimer::singleShot(0, a_mainWindow, [this, a_mainWindow] {
            if (auto* const api = DevBenchAPI::GetDevBenchInterface001()) {
                _devBench = DevBench::Tool::Register(*api, *this, a_mainWindow);
            }
        });
    });
    return true;
}

QString PreviewNif::name() const {
    return "Preview NIF";
}

QString PreviewNif::author() const {
    return "Parapets";
}

QString PreviewNif::description() const {
    return "Supports previewing NIF files";
}

MOBase::VersionInfo PreviewNif::version() const {
    return {MOPK_VERSION_MAJOR, MOPK_VERSION_MINOR, MOPK_VERSION_PATCH, MOBase::VersionInfo::RELEASE_BETA};
}

QList<MOBase::PluginSetting> PreviewNif::settings() const {
    return {};
}

bool PreviewNif::enabledByDefault() const {
    return true;
}

std::set<QString> PreviewNif::supportedExtensions() const {
    return MeshFiles::Extensions();
}

bool PreviewNif::supportsArchives() const {
    return true;
}

QWidget* PreviewNif::genFilePreview(const QString& a_fileName, const QSize& a_maxSize) const {
    Q_UNUSED(a_maxSize);

    return CreatePreview(a_fileName, {});
}

QWidget* PreviewNif::genDataPreview(
    const QByteArray& a_fileData,
    const QString& a_fileName,
    const QSize& a_maxSize
) const {
    Q_UNUSED(a_maxSize);

    return CreatePreview(a_fileName, a_fileData);
}

NifPreviewWidget* PreviewNif::CreatePreview(
    const QString& a_fileName,
    const QByteArray& a_fileData,
    QSharedPointer<Camera> a_camera
) const {
    NifPreviewResolver resolver = [this, organizer = _moInfo, a_fileName, a_fileData] {
        auto gameFiles = SnapshotGameFiles();
        auto sourceSet = NifPreviewSourceResolver::Resolve(organizer, *gameFiles, a_fileName, a_fileData);
        return NifPreviewResolution {.gameFiles = std::move(gameFiles), .sourceSet = std::move(sourceSet)};
    };

    auto resolution = resolver();
    if (resolution.sourceSet.providers.isEmpty()) {
        qWarning("Failed to find previewable NIF provider for '%s'", qUtf8Printable(a_fileName));
        return nullptr;
    }
    return new NifPreviewWidget(
        std::move(resolution),
        std::move(resolver),
        *_moInfo,
        a_camera ? std::move(a_camera) : SharedCamera()
    );
}

std::shared_ptr<const GameFiles> PreviewNif::SnapshotGameFiles() const {
    return std::make_shared<const GameFiles>(*_moInfo, *_archiveIndex);
}

MeshBrowser* PreviewNif::Browser() const {
    return _browser;
}

MeshBrowser* PreviewNif::ShowBrowser(QWidget* a_parent) const {
    if (_browser) {
        _browser->raise();
        _browser->activateWindow();
        return _browser;
    }

    if (!_meshCatalog) {
        _meshCatalog = std::make_unique<MeshCatalog>(*_moInfo, *_archiveIndex);
    }
    _browser = new MeshBrowser(*this, *_meshCatalog, *_moInfo, a_parent);
    _browser->show();
    return _browser;
}

QSharedPointer<Camera> PreviewNif::SharedCamera() const {
    auto camera = _sharedCamera.toStrongRef();
    if (camera.isNull()) {
        camera = {new Camera(), &Camera::deleteLater};
        _sharedCamera = camera;
    }

    return camera;
}
