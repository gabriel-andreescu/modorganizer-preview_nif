#pragma once

#include <QByteArray>
#include <QVariant>

namespace MOBase {
class IOrganizer;
}

// The preview's choices that persist per MO2 profile, in the profile's preview_nif.ini.
class PreviewPreferences {
public:
    explicit PreviewPreferences(const MOBase::IOrganizer& a_organizer);

    [[nodiscard]] bool SplitPreview() const;
    void SetSplitPreview(bool a_enabled) const;
    [[nodiscard]] bool ShowTextures() const;
    void SetShowTextures(bool a_show) const;
    [[nodiscard]] bool PlayAnimation() const;
    void SetPlayAnimation(bool a_play) const;
    // The NIF Browser window's saved geometry and the state of its splitter. Empty until first saved.
    [[nodiscard]] QByteArray BrowserGeometry() const;
    void SetBrowserGeometry(const QByteArray& a_geometry) const;
    [[nodiscard]] QByteArray BrowserSplitter() const;
    void SetBrowserSplitter(const QByteArray& a_state) const;

private:
    [[nodiscard]] QVariant Value(const char* a_key, const QVariant& a_default) const;
    void SetValue(const char* a_key, const QVariant& a_value) const;

    const MOBase::IOrganizer* _organizer;
};
