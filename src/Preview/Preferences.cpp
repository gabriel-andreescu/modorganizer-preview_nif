#include "Preview/Preferences.h"

#include <QDir>
#include <QSettings>

#include <uibase/imoinfo.h>

namespace {
constexpr auto kFileName = "preview_nif.ini";
constexpr auto kGroup = "Preview";
constexpr auto kSplitPreviewKey = "splitPreview";
constexpr auto kShowTexturesKey = "showTextures";
constexpr auto kBrowserGeometryKey = "browserGeometry";
constexpr auto kBrowserSplitterKey = "browserSplitter";
} // namespace

PreviewPreferences::PreviewPreferences(const MOBase::IOrganizer& a_organizer)
    : _organizer(&a_organizer) {}

bool PreviewPreferences::SplitPreview() const {
    return Value(kSplitPreviewKey, false).toBool();
}

void PreviewPreferences::SetSplitPreview(const bool a_enabled) const {
    SetValue(kSplitPreviewKey, a_enabled);
}

bool PreviewPreferences::ShowTextures() const {
    return Value(kShowTexturesKey, true).toBool();
}

void PreviewPreferences::SetShowTextures(const bool a_show) const {
    SetValue(kShowTexturesKey, a_show);
}

QByteArray PreviewPreferences::BrowserGeometry() const {
    return Value(kBrowserGeometryKey, {}).toByteArray();
}

void PreviewPreferences::SetBrowserGeometry(const QByteArray& a_geometry) const {
    SetValue(kBrowserGeometryKey, a_geometry);
}

QByteArray PreviewPreferences::BrowserSplitter() const {
    return Value(kBrowserSplitterKey, {}).toByteArray();
}

void PreviewPreferences::SetBrowserSplitter(const QByteArray& a_state) const {
    SetValue(kBrowserSplitterKey, a_state);
}

QVariant PreviewPreferences::Value(const char* a_key, const QVariant& a_default) const {
    const QSettings settings(QDir(_organizer->profilePath()).filePath(kFileName), QSettings::IniFormat);
    return settings.value(QStringLiteral("%1/%2").arg(QLatin1StringView(kGroup), QLatin1StringView(a_key)), a_default);
}

void PreviewPreferences::SetValue(const char* a_key, const QVariant& a_value) const {
    QSettings settings(QDir(_organizer->profilePath()).filePath(kFileName), QSettings::IniFormat);
    settings.setValue(QStringLiteral("%1/%2").arg(QLatin1StringView(kGroup), QLatin1StringView(a_key)), a_value);
}
