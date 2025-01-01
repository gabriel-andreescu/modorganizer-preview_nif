#include "TextureManager.h"
#include "PreviewNif.h"

#include <dataarchives.h>
#include <igamefeatures.h>
#include <iplugingame.h>
#include <ifiletree.h>

#include <gli/gli.hpp>
#include <libbsarch.h>

#include <QFileInfo>
#include <QOpenGLFunctions_2_1>
#include <QOpenGLVersionFunctionsFactory>
#include <QVector4D>

#include <memory>
#include <ranges>

struct BsaPtrDeleter
{
  void operator()(void* ptr) const
  {
    bsa_free(ptr);
  }
};

using UniqueBsaPtr = std::unique_ptr<void, BsaPtrDeleter>;

struct BsaBufferDeleter
{
  explicit BsaBufferDeleter(void* bsa) : m_bsa(bsa)
  {
  }

  void operator()(const bsa_result_buffer_t* buffer) const
  {
    bsa_file_data_free(m_bsa, *buffer);
  }

  void* m_bsa;
};

using UniqueBufferPtr = std::unique_ptr<bsa_result_buffer_t, BsaBufferDeleter>;

TextureManager::TextureManager(MOBase::IOrganizer* organizer)
  : m_MOInfo{organizer}
{
}

void TextureManager::cleanup()
{
  for (auto it = m_Textures.cbegin(); it != m_Textures.cend();) {
    const auto* texture = it->second;
    m_Textures.erase(it++);
    delete texture;
  }

  auto cleanupTexture = [&](QOpenGLTexture*& texPtr) {
    if (texPtr) {
      delete texPtr;
      texPtr = nullptr;
    }
  };

  cleanupTexture(m_ErrorTexture);
  cleanupTexture(m_BlackTexture);
  cleanupTexture(m_WhiteTexture);
  cleanupTexture(m_FlatNormalTexture);
}

QOpenGLTexture* TextureManager::getTexture(const std::string& texturePath)
{
  return getTexture(QString::fromStdString(texturePath));
}

QOpenGLTexture* TextureManager::getTexture(const QString& texturePath)
{
  if (texturePath.isEmpty()) {
    return nullptr;
  }

  const auto key = texturePath.toLower().toStdWString();

  if (const auto it = m_Textures.find(key); it != m_Textures.end()) {
    return it->second;
  }

  QOpenGLTexture* texture = loadTexture(texturePath);
  m_Textures[key]         = texture;
  return texture;
}

QOpenGLTexture* TextureManager::getErrorTexture()
{
  if (!m_ErrorTexture) {
    m_ErrorTexture = makeSolidColor({1.0f, 0.0f, 1.0f, 1.0f});
  }
  return m_ErrorTexture;
}

QOpenGLTexture* TextureManager::getBlackTexture()
{
  if (!m_BlackTexture) {
    m_BlackTexture = makeSolidColor({0.0f, 0.0f, 0.0f, 1.0f});
  }
  return m_BlackTexture;
}

QOpenGLTexture* TextureManager::getWhiteTexture()
{
  if (!m_WhiteTexture) {
    m_WhiteTexture = makeSolidColor({1.0f, 1.0f, 1.0f, 1.0f});
  }
  return m_WhiteTexture;
}

QOpenGLTexture* TextureManager::getFlatNormalTexture()
{
  if (!m_FlatNormalTexture) {
    m_FlatNormalTexture = makeSolidColor({0.5f, 0.5f, 1.0f, 1.0f});
  }
  return m_FlatNormalTexture;
}

QOpenGLTexture* TextureManager::loadTexture(const QString& texturePath) const
{
  if (texturePath.isEmpty()) {
    return nullptr;
  }

  const auto game = m_MOInfo->managedGame();
  if (!game) {
    qCritical("Failed to interface with managed game plugin");
    return nullptr;
  }

  const auto realPath   = resolvePath(game, texturePath);
  const bool fileExists =
      !realPath.isEmpty() && QFileInfo::exists(realPath) && QFileInfo(realPath).
      isFile();

  if (fileExists) {
    return makeTexture(gli::load(realPath.toStdString()));
  }

  if (const auto texture = tryLoadTextureFromMods(texturePath)) {
    return texture;
  }

  if (const auto texture = tryLoadTextureFromGame(texturePath)) {
    return texture;
  }

  return nullptr;
}

QOpenGLTexture* TextureManager::tryLoadTextureFromMods(const QString& texturePath) const
{
  const auto fileOrigins = m_MOInfo->getFileOrigins(texturePath);
  if (fileOrigins.empty()) {
    return nullptr;
  }

  const auto& modName = fileOrigins.constFirst();
  if (const auto mod = m_MOInfo->modList()->getMod(modName)) {
    if (const auto fileTree = mod->fileTree()) {
      for (auto it = fileTree->begin(); it != fileTree->end(); ++it) {
        const auto fileInfo = *it;
        if (!fileInfo) {
          continue;
        }
        if (!fileInfo->name().endsWith(".bsa", Qt::CaseInsensitive)) {
          continue;
        }

        const auto bsaPath = resolvePath(m_MOInfo->managedGame(), fileInfo->name());
        if (bsaPath.isEmpty()) {
          continue;
        }
        if (const auto texture = loadTextureFromBSA(bsaPath, texturePath)) {
          return texture;
        }
      }
    }
  }
  return nullptr;
}

QOpenGLTexture* TextureManager::tryLoadTextureFromGame(
    const QString& texturePath) const
{
  const auto features     = m_MOInfo->gameFeatures();
  const auto gameArchives = features->gameFeature<MOBase::DataArchives>();
  if (!gameArchives) {
    return nullptr;
  }

  for (auto archives = gameArchives->archives(m_MOInfo->profile());
       const auto& archive : std::ranges::reverse_view(archives)) {
    const auto bsaPath = resolvePath(m_MOInfo->managedGame(), archive);
    if (bsaPath.isEmpty()) {
      continue;
    }
    if (const auto texture = loadTextureFromBSA(bsaPath, texturePath)) {
      return texture;
    }
  }
  return nullptr;
}

QOpenGLTexture* TextureManager::loadTextureFromBSA(const QString& bsaPath,
                                                   const QString& texturePath)
{
  const UniqueBsaPtr bsaHandle(bsa_create());
  static_assert(sizeof(wchar_t) == 2, "Expected wchar_t to be 2 bytes");

  const auto bsaPathUtf16  = reinterpret_cast<const wchar_t*>(bsaPath.utf16());
  const auto [code, _text] = bsa_load_from_file(bsaHandle.get(), bsaPathUtf16);
  if (code == BSA_RESULT_EXCEPTION) {
    return nullptr;
  }

  const auto texturePathUtf16 = reinterpret_cast<const wchar_t*>(texturePath.utf16());
  auto [rBuffer, msg]         = bsa_extract_file_data_by_filename(
      bsaHandle.get(), texturePathUtf16);
  if (msg.code == BSA_RESULT_EXCEPTION) {
    return nullptr;
  }

  const UniqueBufferPtr buffer(&rBuffer, BsaBufferDeleter(bsaHandle.get()));

  const auto data = static_cast<char*>(buffer->data);
  return makeTexture(gli::load(data, buffer->size));
}

QOpenGLTexture* TextureManager::makeTexture(const gli::texture& texture)
{
  if (texture.empty()) {
    return nullptr;
  }

  const gli::gl GL(gli::gl::PROFILE_GL32);
  const auto [internal, external, type, swizzles] =
      GL.translate(texture.format(), texture.swizzles());
  GLenum target = GL.translate(texture.target());

  auto* f = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_2_1>(
      QOpenGLContext::currentContext());
  auto* glTexture = new QOpenGLTexture(static_cast<QOpenGLTexture::Target>(target));

  glTexture->create();
  glTexture->bind();
  glTexture->setMipLevels(static_cast<int>(texture.levels()));
  glTexture->setMipBaseLevel(0);
  glTexture->setMipMaxLevel(static_cast<int>(texture.levels()) - 1);
  glTexture->setMinMagFilters(QOpenGLTexture::LinearMipMapLinear,
                              QOpenGLTexture::Linear);
  glTexture->setSwizzleMask(static_cast<QOpenGLTexture::SwizzleValue>(swizzles[0]),
                            static_cast<QOpenGLTexture::SwizzleValue>(swizzles[1]),
                            static_cast<QOpenGLTexture::SwizzleValue>(swizzles[2]),
                            static_cast<QOpenGLTexture::SwizzleValue>(swizzles[3]));
  glTexture->setWrapMode(QOpenGLTexture::Repeat);

  const auto extent = texture.extent();
  glTexture->setSize(extent.x, extent.y, extent.z);
  glTexture->setFormat(static_cast<QOpenGLTexture::TextureFormat>(internal));
  glTexture->allocateStorage(static_cast<QOpenGLTexture::PixelFormat>(external),
                             static_cast<QOpenGLTexture::PixelType>(type));

  for (std::size_t layer = 0; layer < texture.layers(); layer++) {
    for (std::size_t face = 0; face < texture.faces(); face++) {
      for (std::size_t level = 0; level < texture.levels(); level++) {
        const auto levelExtent = texture.extent(level);

        const GLenum targetFace = is_target_cube(texture.target())
                                    ? (GL_TEXTURE_CUBE_MAP_POSITIVE_X + face)
                                    : target;

        const auto dataPtr = texture.data(layer, face, level);

        if (is_compressed(texture.format())) {
          switch (texture.target()) {
          case gli::TARGET_1D:
            f->glCompressedTexSubImage1D(
                targetFace, static_cast<GLint>(level), 0,
                levelExtent.x, internal, static_cast<GLsizei>(texture.size(level)),
                dataPtr);
            break;
          case gli::TARGET_1D_ARRAY:
          case gli::TARGET_2D:
          case gli::TARGET_CUBE:
            f->glCompressedTexSubImage2D(
                targetFace, static_cast<GLint>(level), 0, 0, levelExtent.x,
                (texture.target() == gli::TARGET_1D_ARRAY)
                  ? static_cast<GLint>(layer)
                  : levelExtent.y,
                internal, static_cast<GLsizei>(texture.size(level)), dataPtr);
            break;
          case gli::TARGET_2D_ARRAY:
          case gli::TARGET_3D:
          case gli::TARGET_CUBE_ARRAY:
            f->glCompressedTexSubImage3D(
                targetFace, static_cast<GLint>(level), 0, 0, 0, levelExtent.x,
                levelExtent.y,
                (texture.target() == gli::TARGET_3D)
                  ? levelExtent.z
                  : static_cast<GLint>(layer),
                internal, static_cast<GLsizei>(texture.size(level)), dataPtr);
            break;
          default:
            break;
          }
        } else {
          switch (texture.target()) {
          case gli::TARGET_1D:
            f->glTexSubImage1D(
                targetFace, static_cast<GLint>(level), 0, levelExtent.x,
                external, type, dataPtr);
            break;
          case gli::TARGET_1D_ARRAY:
          case gli::TARGET_2D:
          case gli::TARGET_CUBE:
            f->glTexSubImage2D(
                targetFace, static_cast<GLint>(level), 0, 0, levelExtent.x,
                (texture.target() == gli::TARGET_1D_ARRAY)
                  ? static_cast<GLint>(layer)
                  : levelExtent.y,
                external, type, dataPtr);
            break;
          case gli::TARGET_2D_ARRAY:
          case gli::TARGET_3D:
          case gli::TARGET_CUBE_ARRAY:
            f->glTexSubImage3D(
                targetFace, static_cast<GLint>(level), 0, 0, 0, levelExtent.x,
                levelExtent.y,
                (texture.target() == gli::TARGET_3D)
                  ? levelExtent.z
                  : static_cast<GLint>(layer),
                external, type, dataPtr);
            break;
          default:
            break;
          }
        }
      }
    }
  }

  glTexture->release();
  return glTexture;
}

QOpenGLTexture* TextureManager::makeSolidColor(const QVector4D color)
{
  auto* glTexture = new QOpenGLTexture(QOpenGLTexture::Target2D);
  glTexture->create();
  glTexture->bind();

  glTexture->setSize(1, 1);
  glTexture->setFormat(QOpenGLTexture::RGBA32F);
  glTexture->allocateStorage(QOpenGLTexture::RGBA, QOpenGLTexture::Float32);

  glTexture->setData(QOpenGLTexture::RGBA, QOpenGLTexture::Float32, &color);

  glTexture->release();
  return glTexture;
}

QString TextureManager::resolvePath(const MOBase::IPluginGame* game,
                                    const QString& path) const
{
  if (auto resolved = m_MOInfo->resolvePath(path); !resolved.isEmpty()) {
    return resolved;
  }

  const auto dataPath =
      game->dataDirectory().absoluteFilePath(QDir::cleanPath(path));

  return QFileInfo::exists(dataPath) ? dataPath : QString();
}
