#pragma once

#include "Textures/PreviewTexture.h"

#include <QVector4D>

#include <map>
#include <memory>
#include <string>

class QString;

class TextureCache {
public:
    void Cleanup();

    [[nodiscard]] bool ContainsTexture(const QString& a_texturePath) const;
    [[nodiscard]] PreviewTexture* Texture(const QString& a_texturePath) const;
    PreviewTexture* StoreTexture(const QString& a_texturePath, std::unique_ptr<PreviewTexture> a_texture);

    PreviewTexture* GetErrorTexture();
    PreviewTexture* GetBlackTexture();
    PreviewTexture* GetWhiteTexture();
    PreviewTexture* GetFlatNormalTexture();

private:
    [[nodiscard]] static std::wstring CacheKey(const QString& a_texturePath);
    static void DestroyTexture(std::unique_ptr<PreviewTexture>& a_texture);
    static PreviewTexture* GetFallbackTexture(std::unique_ptr<PreviewTexture>& a_texture, QVector4D a_color);

    std::map<std::wstring, std::unique_ptr<PreviewTexture>> _textures;
    std::unique_ptr<PreviewTexture> _errorTexture;
    std::unique_ptr<PreviewTexture> _blackTexture;
    std::unique_ptr<PreviewTexture> _whiteTexture;
    std::unique_ptr<PreviewTexture> _flatNormalTexture;
};
