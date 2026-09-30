#include "PreviewNif.h"
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

PreviewNif::PreviewNif() = default;
PreviewNif::~PreviewNif() = default;

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
    return {"bto", "btr", "nif"};
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

NifPreviewWidget* PreviewNif::CreatePreview(const QString& a_fileName, const QByteArray& a_fileData) const {
    auto sourceSet = NifPreviewSourceResolver::Resolve(_moInfo, a_fileName, a_fileData);
    if (sourceSet.providers.isEmpty()) {
        qWarning("Failed to find previewable NIF provider for '%s'", qUtf8Printable(a_fileName));
        return nullptr;
    }

    const auto lastProviderIndex = static_cast<int>(sourceSet.providers.size()) - 1;
    sourceSet.currentIndex = std::clamp(sourceSet.currentIndex, 0, lastProviderIndex);
    return new NifPreviewWidget(std::move(sourceSet), _moInfo, SharedCamera());
}

QSharedPointer<Camera> PreviewNif::SharedCamera() const {
    auto camera = _sharedCamera.toStrongRef();
    if (camera.isNull()) {
        camera = {new Camera(), &Camera::deleteLater};
        _sharedCamera = camera;
    }

    return camera;
}
