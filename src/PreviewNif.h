#pragma once

#include <QSharedPointer>
#include <QWeakPointer>
#include <uibase/ipluginpreview.h>

#include <memory>

class Camera;
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

    // IPlugin Interface

    bool init(MOBase::IOrganizer* a_moInfo) override;
    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString author() const override;
    [[nodiscard]] QString description() const override;
    [[nodiscard]] MOBase::VersionInfo version() const override;
    [[nodiscard]] QList<MOBase::PluginSetting> settings() const override;
    [[nodiscard]] bool enabledByDefault() const override;

    // IPluginPreview interface

    [[nodiscard]] std::set<QString> supportedExtensions() const override;
    [[nodiscard]] bool supportsArchives() const override;
    [[nodiscard]] QWidget* genFilePreview(const QString& a_fileName, const QSize& a_maxSize) const override;
    [[nodiscard]] QWidget* genDataPreview(
        const QByteArray& a_fileData,
        const QString& a_fileName,
        const QSize& a_maxSize
    ) const override;

    // Returns null when no provider supplies a previewable NIF for the file.
    [[nodiscard]] NifPreviewWidget* CreatePreview(const QString& a_fileName, const QByteArray& a_fileData) const;

private:
    [[nodiscard]] QSharedPointer<Camera> SharedCamera() const;

    MOBase::IOrganizer* _moInfo {};
    mutable QWeakPointer<Camera> _sharedCamera;
    std::unique_ptr<DevBench::Tool> _devBench;
};
