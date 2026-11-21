#pragma once

#include <uibase/iplugintool.h>

// Adds NIF Browser to MO2's Tools menu. The browser itself lives in preview_nif.dll, because MO2 allows one plugin
// type per DLL.
class NifBrowserTool final : public MOBase::IPluginTool {
    Q_OBJECT
    Q_INTERFACES(MOBase::IPlugin MOBase::IPluginTool)
    Q_PLUGIN_METADATA(IID "org.tannin.PreviewNifBrowser")

public:
    bool init(MOBase::IOrganizer* a_organizer) override;
    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString localizedName() const override;
    [[nodiscard]] QString author() const override;
    [[nodiscard]] QString description() const override;
    [[nodiscard]] MOBase::VersionInfo version() const override;
    [[nodiscard]] QList<MOBase::PluginSetting> settings() const override;
    [[nodiscard]] std::vector<std::shared_ptr<const MOBase::IPluginRequirement>> requirements() const override;

    [[nodiscard]] QString displayName() const override;
    [[nodiscard]] QString tooltip() const override;
    [[nodiscard]] QIcon icon() const override;
    void display() const override;
};
