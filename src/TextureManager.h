#pragma once

#include "OpenGLResources.h"
#include "TextureSource.h"

#include <gli/gli.hpp>
#include <map>
#include <uibase/imoinfo.h>

class PreviewTexture {
public:
    explicit PreviewTexture(QOpenGLTexture* texture);
    PreviewTexture(GLuint textureId, GLenum target);
    ~PreviewTexture();
    PreviewTexture(const PreviewTexture&) = delete;
    PreviewTexture(PreviewTexture&&) = delete;
    PreviewTexture& operator=(const PreviewTexture&) = delete;
    PreviewTexture& operator=(PreviewTexture&&) = delete;

    void bind(int textureUnit) const;
    void destroyWithCurrentContext();

private:
    QtOpenGLTextureResource m_QtTexture;
    OpenGLTextureResource m_RawTexture;
};

class TextureManager {
public:
    explicit TextureManager(MOBase::IOrganizer* organizer, TextureSourceProvider textureSource = {});
    ~TextureManager() = default;
    TextureManager(const TextureManager&) = delete;
    TextureManager(TextureManager&&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;
    TextureManager& operator=(TextureManager&&) = delete;

    void cleanup();

    PreviewTexture* getTexture(const std::string& texturePath);
    PreviewTexture* getTexture(const QString& texturePath);

    PreviewTexture* getErrorTexture();
    PreviewTexture* getBlackTexture();
    PreviewTexture* getWhiteTexture();
    PreviewTexture* getFlatNormalTexture();

private:
    [[nodiscard]] PreviewTexture* loadTexture(const QString& texturePath) const;
    PreviewTexture* loadTextureAuto(const QString& texturePath) const;
    PreviewTexture* tryLoadTextureFromSource(const QString& texturePath) const;
    static PreviewTexture* loadLooseTexture(const QString& path);
    static PreviewTexture* tryLoadTextureFromArchives(const QStringList& archivePaths, const QString& texturePath);
    PreviewTexture* tryLoadTextureFromMods(const QString& texturePath) const;
    PreviewTexture* tryLoadTextureFromGame(const QString& texturePath) const;
    static PreviewTexture* loadTextureFromArchive(const QString& archivePath, const QString& texturePath);
    static PreviewTexture* makeTexture(const gli::texture& texture);
    static PreviewTexture* makeSolidColor(QVector4D color);

    MOBase::IOrganizer* m_MOInfo;
    TextureSourceProvider m_TextureSource;
    PreviewTexture* m_ErrorTexture = nullptr;
    PreviewTexture* m_BlackTexture = nullptr;
    PreviewTexture* m_WhiteTexture = nullptr;
    PreviewTexture* m_FlatNormalTexture = nullptr;

    std::map<std::wstring, PreviewTexture*> m_Textures;
};
