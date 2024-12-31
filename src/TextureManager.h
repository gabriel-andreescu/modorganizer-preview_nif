#pragma once

#include <QOpenGLTexture>
#include <gli/gli.hpp>
#include <imoinfo.h>
#include <map>

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

  QOpenGLTexture* getTexture(const std::string& texturePath);
  QOpenGLTexture* getTexture(const QString& texturePath);

  QOpenGLTexture* getErrorTexture();
  QOpenGLTexture* getBlackTexture();
  QOpenGLTexture* getWhiteTexture();
  QOpenGLTexture* getFlatNormalTexture();

private:
  [[nodiscard]] QOpenGLTexture* loadTexture(const QString& texturePath) const;
  static QOpenGLTexture* makeTexture(const gli::texture& texture);
  static QOpenGLTexture* makeSolidColor(QVector4D color);

  QString resolvePath(const MOBase::IPluginGame* game, const QString& path) const;

  MOBase::IOrganizer* m_MOInfo;
  QOpenGLTexture* m_ErrorTexture      = nullptr;
  QOpenGLTexture* m_BlackTexture      = nullptr;
  QOpenGLTexture* m_WhiteTexture      = nullptr;
  QOpenGLTexture* m_FlatNormalTexture = nullptr;

  std::map<std::wstring, QOpenGLTexture*> m_Textures;
};
