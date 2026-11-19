#include "Preview/Preferences.h"

#include <QDir>
#include <QSettings>

#include <uibase/imoinfo.h>

namespace {
constexpr auto kFileName = "preview_nif.ini";
constexpr auto kGroup = "Preview";
constexpr auto kSplitPreviewKey = "splitPreview";
constexpr auto kShowTexturesKey = "showTextures";
} // namespace

PreviewPreferences::PreviewPreferences(const MOBase::IOrganizer& a_organizer)
    : _organizer(&a_organizer) {}

bool PreviewPreferences::SplitPreview() const {
    return Value(kSplitPreviewKey, false);
}

void PreviewPreferences::SetSplitPreview(const bool a_enabled) const {
    SetValue(kSplitPreviewKey, a_enabled);
}

bool PreviewPreferences::ShowTextures() const {
    return Value(kShowTexturesKey, true);
}

void PreviewPreferences::SetShowTextures(const bool a_show) const {
    SetValue(kShowTexturesKey, a_show);
}

bool PreviewPreferences::Value(const char* a_key, const bool a_default) const {
    QSettings settings(QDir(_organizer->profilePath()).filePath(kFileName), QSettings::IniFormat);
    settings.beginGroup(kGroup);
    return settings.value(a_key, a_default).toBool();
}

void PreviewPreferences::SetValue(const char* a_key, const bool a_value) const {
    QSettings settings(QDir(_organizer->profilePath()).filePath(kFileName), QSettings::IniFormat);
    settings.beginGroup(kGroup);
    settings.setValue(a_key, a_value);
}
