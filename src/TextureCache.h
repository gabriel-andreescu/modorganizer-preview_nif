#pragma once

#include "PreviewTexture.h"

#include <QVector4D>

#include <map>
#include <memory>
#include <string>

class QString;

class TextureCache {
public:
    void cleanup();

    [[nodiscard]] bool containsTexture(const QString& texturePath) const;
    [[nodiscard]] PreviewTexture* texture(const QString& texturePath) const;
    PreviewTexture* storeTexture(const QString& texturePath, std::unique_ptr<PreviewTexture> texture);

    PreviewTexture* getErrorTexture();
    PreviewTexture* getBlackTexture();
    PreviewTexture* getWhiteTexture();
    PreviewTexture* getFlatNormalTexture();

private:
    [[nodiscard]] static std::wstring cacheKey(const QString& texturePath);
    static void destroyTexture(std::unique_ptr<PreviewTexture>& texture);
    static PreviewTexture* getFallbackTexture(std::unique_ptr<PreviewTexture>& texture, QVector4D color);

    std::map<std::wstring, std::unique_ptr<PreviewTexture>> m_Textures;
    std::unique_ptr<PreviewTexture> m_ErrorTexture;
    std::unique_ptr<PreviewTexture> m_BlackTexture;
    std::unique_ptr<PreviewTexture> m_WhiteTexture;
    std::unique_ptr<PreviewTexture> m_FlatNormalTexture;
};
