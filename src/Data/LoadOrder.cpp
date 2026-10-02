#include "Data/LoadOrder.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QStringList>

#include <algorithm>
#include <memory>
#include <ranges>
#include <uibase/game_features/dataarchives.h>
#include <uibase/game_features/igamefeatures.h>
#include <uibase/imoinfo.h>
#include <uibase/iplugingame.h>
#include <uibase/ipluginlist.h>

namespace {
const MOBase::IProfile* ProfilePointer(const MOBase::IProfile* a_profile) {
    return a_profile;
}

template <class Profile>
const MOBase::IProfile* ProfilePointer(const std::shared_ptr<Profile>& a_profile) {
    return a_profile.get();
}

// For each active plugin, Skyrim SE and VR register "<plugin>.bsa" then "<plugin> - Textures.bsa", the original
// Skyrim engine only "<plugin>.bsa", and Fallout 4 "<plugin> - Main.ba2" then "<plugin> - Textures.ba2". An archive
// already registered is skipped.
QStringList PluginArchiveSuffixes(const QString& a_gameShortName) {
    static const QStringList kSkyrimSeGames {"SkyrimSE", "SkyrimVR", "EnderalSE"};
    static const QStringList kSkyrimLeGames {"Skyrim", "Enderal"};
    static const QStringList kFallout4Games {"Fallout4", "Fallout4VR", "Fallout4London"};
    if (kSkyrimSeGames.contains(a_gameShortName, Qt::CaseInsensitive)) {
        return {".bsa", " - Textures.bsa"};
    }
    if (kSkyrimLeGames.contains(a_gameShortName, Qt::CaseInsensitive)) {
        return {".bsa"};
    }
    if (kFallout4Games.contains(a_gameShortName, Qt::CaseInsensitive)) {
        return {" - Main.ba2", " - Textures.ba2"};
    }
    return {};
}

// Games without a verified rule get MO2's own: any archive named "<plugin> - *" or "<plugin>.*".
QStringList ArchivesNamedAfter(const QString& a_pluginStem, const QStringList& a_archiveNames) {
    QStringList names;
    for (const auto& name : a_archiveNames) {
        if (name.startsWith(a_pluginStem + QStringLiteral(" - "), Qt::CaseInsensitive)
            || name.startsWith(a_pluginStem + QStringLiteral("."), Qt::CaseInsensitive)) {
            names.append(name);
        }
    }
    return names;
}

QStringList PluginArchiveNames(
    const QString& a_pluginStem,
    const QStringList& a_suffixes,
    const QStringList& a_dataArchiveNames
) {
    if (a_suffixes.isEmpty()) {
        return ArchivesNamedAfter(a_pluginStem, a_dataArchiveNames);
    }

    QStringList names;
    for (const auto& suffix : a_suffixes) {
        names.append(a_pluginStem + suffix);
    }
    return names;
}

QStringList DataArchiveNames(const MOBase::IOrganizer& a_organizer) {
    QStringList names;
    for (const auto& path : a_organizer.findFiles({}, QStringList {"*.bsa", "*.ba2"})) {
        names.append(QFileInfo(path).fileName());
    }
    return names;
}

// The game's INI archive lists load before any plugin's archives. The profile's INI can omit them, and then the
// game falls back to its default list, which MO2 reports as the vanilla archives.
QStringList IniArchives(const MOBase::IOrganizer& a_organizer) {
    QStringList names;
    const auto* const features = a_organizer.gameFeatures();
    const auto dataArchives = features ? features->gameFeature<MOBase::DataArchives>() : nullptr;
    if (!dataArchives) {
        return names;
    }

    names.append(dataArchives->archives(ProfilePointer(a_organizer.profile())));
    names.append(dataArchives->vanillaArchives());
    return names;
}

QStringList ActivePluginsInLoadOrder(const MOBase::IOrganizer& a_organizer) {
    auto* const pluginList = a_organizer.pluginList();
    if (!pluginList) {
        return {};
    }

    QStringList plugins;
    for (const auto& plugin : pluginList->pluginNames()) {
        if (pluginList->state(plugin) == MOBase::IPluginList::STATE_ACTIVE) {
            plugins.append(plugin);
        }
    }
    std::ranges::sort(plugins, {}, [&](const QString& a_plugin) { return pluginList->loadOrder(a_plugin); });
    return plugins;
}

QString ArchivePath(const MOBase::IOrganizer& a_organizer, const QString& a_name) {
    if (const auto resolved = a_organizer.resolvePath(a_name); !resolved.isEmpty()) {
        return QDir::fromNativeSeparators(QFileInfo(resolved).absoluteFilePath());
    }

    const auto* const game = a_organizer.managedGame();
    const QFileInfo dataFile(game ? game->dataDirectory().absoluteFilePath(a_name) : QString());
    return dataFile.isFile() ? QDir::fromNativeSeparators(dataFile.absoluteFilePath()) : QString();
}

QString ArchiveModName(const MOBase::IOrganizer& a_organizer, const QString& a_name) {
    const auto origins = a_organizer.getFileOrigins(a_name);
    if (origins.isEmpty() || origins.constFirst().compare(QStringLiteral("data"), Qt::CaseInsensitive) == 0) {
        return {};
    }
    return origins.constFirst();
}
} // namespace

namespace LoadOrder {

QVector<LoadedArchive> LoadedArchives(const MOBase::IOrganizer& a_organizer) {
    QVector<LoadedArchive> archives;
    QSet<QString> registered;
    const auto registerArchive = [&](const QString& a_name, const QString& a_plugin) {
        const auto key = a_name.toLower();
        if (registered.contains(key)) {
            return;
        }
        registered.insert(key);

        if (auto path = ArchivePath(a_organizer, a_name); !path.isEmpty()) {
            archives.push_back({
                .path = std::move(path),
                .modName = ArchiveModName(a_organizer, a_name),
                .plugin = a_plugin,
            });
        }
    };

    for (const auto& name : IniArchives(a_organizer)) {
        registerArchive(name, {});
    }

    const auto* const game = a_organizer.managedGame();
    const auto suffixes = PluginArchiveSuffixes(game ? game->gameShortName() : QString());
    const auto dataArchiveNames = suffixes.isEmpty() ? DataArchiveNames(a_organizer) : QStringList();
    for (const auto& plugin : ActivePluginsInLoadOrder(a_organizer)) {
        for (const auto& name : PluginArchiveNames(QFileInfo(plugin).completeBaseName(), suffixes, dataArchiveNames)) {
            registerArchive(name, plugin);
        }
    }

    return archives;
}

}
