#pragma once

#include <QOpenGLTexture>
#include <gli/gli.hpp>
#include <map>
#include <uibase/imoinfo.h>

class PreviewTexture
{
public:
  explicit PreviewTexture(QOpenGLTexture* texture);
  PreviewTexture(GLuint textureId, GLenum target);
  ~PreviewTexture();
  PreviewTexture(const PreviewTexture&)            = delete;
  PreviewTexture(PreviewTexture&&)                 = delete;
  PreviewTexture& operator=(const PreviewTexture&) = delete;
  PreviewTexture& operator=(PreviewTexture&&)      = delete;

  void bind(int textureUnit) const;

private:
  QOpenGLTexture* m_QtTexture = nullptr;
  GLuint m_TextureId          = 0;
  GLenum m_Target             = 0;
};

class TextureManager
{
public:
  explicit TextureManager(MOBase::IOrganizer* organizer);
  ~TextureManager()                                = default;
  TextureManager(const TextureManager&)            = delete;
  TextureManager(TextureManager&&)                 = delete;
  TextureManager& operator=(const TextureManager&) = delete;
  TextureManager& operator=(TextureManager&&)      = delete;

  void cleanup();

  PreviewTexture* getTexture(const std::string& texturePath);
  PreviewTexture* getTexture(const QString& texturePath);

  PreviewTexture* getErrorTexture();
  PreviewTexture* getBlackTexture();
  PreviewTexture* getWhiteTexture();
  PreviewTexture* getFlatNormalTexture();

private:
  [[nodiscard]] PreviewTexture* loadTexture(const QString& texturePath) const;
  PreviewTexture* tryLoadTextureFromMods(const QString& texturePath) const;
  PreviewTexture* tryLoadTextureFromGame(const QString& texturePath) const;
  static PreviewTexture* loadTextureFromBSA(const QString& bsaPath,
                                            const QString& texturePath);
  static PreviewTexture* makeTexture(const gli::texture& texture);
  static PreviewTexture* makeSolidColor(QVector4D color);

  QString resolvePath(const MOBase::IPluginGame* game, const QString& path) const;

  MOBase::IOrganizer* m_MOInfo;
  PreviewTexture* m_ErrorTexture      = nullptr;
  PreviewTexture* m_BlackTexture      = nullptr;
  PreviewTexture* m_WhiteTexture      = nullptr;
  PreviewTexture* m_FlatNormalTexture = nullptr;

  std::map<std::wstring, PreviewTexture*> m_Textures;
};
