#include "NifBrowserTool.h"

#include <QLibrary>
#include <QMessageBox>

#include <uibase/pluginrequirements.h>

namespace {
constexpr auto kPreviewPluginName = "Preview NIF";
constexpr auto kPreviewLibrary = "preview_nif";
constexpr auto kShowBrowserSymbol = "PreviewNifShowBrowser";

using ShowBrowser = void (*)(QWidget*);
} // namespace

bool NifBrowserTool::init(MOBase::IOrganizer* a_organizer) {
    Q_UNUSED(a_organizer);
    return true;
}

QString NifBrowserTool::name() const {
    return "NIF Browser";
}

QString NifBrowserTool::localizedName() const {
    return tr("NIF Browser");
}

QString NifBrowserTool::author() const {
    return "GabonZ";
}

QString NifBrowserTool::description() const {
    return tr("Lists all meshes, including the vanilla ones in the game's archives, and previews them");
}

MOBase::VersionInfo NifBrowserTool::version() const {
    return {MOPK_VERSION_MAJOR, MOPK_VERSION_MINOR, MOPK_VERSION_PATCH, MOBase::VersionInfo::RELEASE_BETA};
}

QList<MOBase::PluginSetting> NifBrowserTool::settings() const {
    return {};
}

std::vector<std::shared_ptr<const MOBase::IPluginRequirement>> NifBrowserTool::requirements() const {
    return {MOBase::PluginRequirementFactory::pluginDependency(kPreviewPluginName)};
}

QString NifBrowserTool::displayName() const {
    return tr("NIF Browser");
}

QString NifBrowserTool::tooltip() const {
    return tr("Browse and preview vanilla and mod meshes");
}

QIcon NifBrowserTool::icon() const {
    return {};
}

void NifBrowserTool::display() const {
    // MO2 has already loaded the library, so this resolves in the loaded module.
    QLibrary library(kPreviewLibrary);
    if (const auto show = reinterpret_cast<ShowBrowser>(library.resolve(kShowBrowserSymbol))) {
        show(parentWidget());
        return;
    }

    QMessageBox::warning(
        parentWidget(),
        tr("NIF Browser"),
        tr("Couldn't open the NIF Browser: %1").arg(library.errorString())
    );
}
