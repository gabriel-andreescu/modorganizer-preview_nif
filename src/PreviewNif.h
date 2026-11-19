#pragma once

#include <QPointer>
#include <QSharedPointer>
#include <QWeakPointer>
#include <uibase/ipluginpreview.h>

#include <memory>

class ArchiveIndex;
class Camera;
class GameFiles;
class MeshBrowser;
class MeshCatalog;
class NifPreviewWidget;

namespace DevBench {
class Tool;
}

class PreviewNif final : public MOBase::IPluginPreview {
    Q_OBJECT
    Q_INTERFACES(MOBase::IPlugin MOBase::IPluginPreview)
    Q_PLUGIN_METADATA(IID "org.tannin.PreviewNif")

public:
    PreviewNif();
    ~PreviewNif() override;
    PreviewNif(const PreviewNif&) = delete;
    PreviewNif(PreviewNif&&) = delete;
    PreviewNif& operator=(const PreviewNif&) = delete;
    PreviewNif& operator=(PreviewNif&&) = delete;

    bool init(MOBase::IOrganizer* a_moInfo) override;
    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString author() const override;
    [[nodiscard]] QString description() const override;
    [[nodiscard]] MOBase::VersionInfo version() const override;
    [[nodiscard]] QList<MOBase::PluginSetting> settings() const override;
    [[nodiscard]] bool enabledByDefault() const override;

    [[nodiscard]] std::set<QString> supportedExtensions() const override;
    [[nodiscard]] bool supportsArchives() const override;
    [[nodiscard]] QWidget* genFilePreview(const QString& a_fileName, const QSize& a_maxSize) const override;
    [[nodiscard]] QWidget* genDataPreview(
        const QByteArray& a_fileData,
        const QString& a_fileName,
        const QSize& a_maxSize
    ) const override;

    // Returns null when no provider supplies a previewable NIF for the file. Without a camera the preview takes the
    // one every preview in MO2's dialog shares, so stepping through a file's copies keeps the view.
    [[nodiscard]] NifPreviewWidget* CreatePreview(
        const QString& a_fileName,
        const QByteArray& a_fileData,
        QSharedPointer<Camera> a_camera = {}
    ) const;
    // The game's data files as MO2 sees them now.
    [[nodiscard]] std::shared_ptr<const GameFiles> SnapshotGameFiles() const;
    // Opens the NIF Browser, or brings it to the front when it is open.
    MeshBrowser* ShowBrowser(QWidget* a_parent) const;
    // Null when it isn't open.
    [[nodiscard]] MeshBrowser* Browser() const;
    // The plugin MO2 loaded, for the NIF Browser tool plugin, which lives in another DLL. Null before MO2 loads it.
    [[nodiscard]] static const PreviewNif* Instance() noexcept {
        return _instance;
    }

private:
    [[nodiscard]] QSharedPointer<Camera> SharedCamera() const;

    static inline const PreviewNif* _instance = nullptr;

    MOBase::IOrganizer* _moInfo {};
    // Shared by every preview, so each archive's member list is read once.
    std::unique_ptr<ArchiveIndex> _archiveIndex;
    mutable QWeakPointer<Camera> _sharedCamera;
    mutable std::unique_ptr<MeshCatalog> _meshCatalog;
    mutable QPointer<MeshBrowser> _browser;
    std::unique_ptr<DevBench::Tool> _devBench;
};
