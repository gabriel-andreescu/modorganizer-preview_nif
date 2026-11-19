#pragma once

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

private:
    [[nodiscard]] bool Value(const char* a_key, bool a_default) const;
    void SetValue(const char* a_key, bool a_value) const;

    const MOBase::IOrganizer* _organizer;
};
