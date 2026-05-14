#include "PreviewNif.h"
#include "Camera.h"
#include "NifPreviewSource.h"
#include "NifPreviewWidget.h"

#include <QDebug>
#include <algorithm>
#include <utility>

bool PreviewNif::init(MOBase::IOrganizer* moInfo) {
    m_MOInfo = moInfo;
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
    return {0, 5, 1, 0, MOBase::VersionInfo::RELEASE_BETA};
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

QWidget* PreviewNif::genFilePreview(const QString& fileName, const QSize& maxSize) const {
    return genDataPreview(nullptr, fileName, maxSize);
}

QWidget* PreviewNif::genDataPreview(const QByteArray& fileData, const QString& fileName, const QSize& maxSize) const {
    Q_UNUSED(maxSize);

    auto sourceSet = NifPreviewSourceResolver::resolve(m_MOInfo, fileName, fileData);
    if (sourceSet.providers.isEmpty()) {
        qWarning("Failed to find previewable NIF provider for '%s'", qUtf8Printable(fileName));
        return nullptr;
    }

    const auto lastProviderIndex = static_cast<int>(sourceSet.providers.size()) - 1;
    sourceSet.currentIndex = std::clamp(sourceSet.currentIndex, 0, lastProviderIndex);
    return new NifPreviewWidget(std::move(sourceSet), m_MOInfo, sharedCamera());
}

QSharedPointer<Camera> PreviewNif::sharedCamera() const {
    auto camera = m_SharedCamera.toStrongRef();
    if (camera.isNull()) {
        camera = {new Camera(), &Camera::deleteLater};
        m_SharedCamera = camera;
    }

    return camera;
}
