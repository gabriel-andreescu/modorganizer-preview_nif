#include "TextureManager.h"
#include "PreviewNif.h"

#include <dataarchives.h>
#include <igamefeatures.h>
#include <iplugingame.h>

#include <gli/gli.hpp>
#include <libbsarch.h>

#include <QOpenGLFunctions_2_1>
#include <QOpenGLVersionFunctionsFactory>
#include <QVector4D>

#include <memory>
#include <ranges>

TextureManager::TextureManager(MOBase::IOrganizer* organizer) : m_MOInfo{organizer} {}

void TextureManager::cleanup()
{
  for (auto it = m_Textures.cbegin(); it != m_Textures.cend();) {
    const auto texture = it->second;
    m_Textures.erase(it++);
    delete texture;
  }

  if (m_ErrorTexture) {
    delete m_ErrorTexture;
    m_ErrorTexture = nullptr;
  }

  if (m_BlackTexture) {
    delete m_BlackTexture;
    m_BlackTexture = nullptr;
  }

  if (m_WhiteTexture) {
    delete m_WhiteTexture;
    m_WhiteTexture = nullptr;
  }

  if (m_FlatNormalTexture) {
    delete m_FlatNormalTexture;
    m_FlatNormalTexture = nullptr;
  }
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

  if (const auto cached = m_Textures.find(key); cached != m_Textures.end()) {
    return cached->second;
  }

  const auto texture = loadTexture(texturePath);

  m_Textures[key] = texture;
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

  const auto game     = m_MOInfo->managedGame();
  const auto features = m_MOInfo->gameFeatures();

  if (!game) {
    qCritical(
        qUtf8Printable(QObject::tr("Failed to interface with managed game plugin")));
    return nullptr;
  }

  if (const auto realPath = resolvePath(game, texturePath); !realPath.isEmpty()) {
    return makeTexture(gli::load(realPath.toStdString()));
  }

  const auto gameArchives = features->gameFeature<MOBase::DataArchives>();
  if (!gameArchives) {
    return nullptr;
  }

  for (auto archives = gameArchives->archives(m_MOInfo->profile());
       const auto& archive : std::ranges::reverse_view(archives)) {
    auto bsaPath = resolvePath(game, archive);
    if (bsaPath.isEmpty()) {
      continue;
    }

    using bsa_ptr = std::unique_ptr<void, decltype(&bsa_free)>;
    auto bsa      = bsa_ptr(bsa_create(), bsa_free);

    static_assert(sizeof(wchar_t) == 2, "Expected wchar_t to be 2 bytes");

    const auto bsaPath_utf16 = reinterpret_cast<const wchar_t*>(bsaPath.utf16());
    if (const auto [code, text] = bsa_load_from_file(bsa.get(), bsaPath_utf16);
        code == BSA_RESULT_EXCEPTION) {
      continue;
    }

    const auto texturePath_utf16 =
        reinterpret_cast<const wchar_t*>(texturePath.utf16());
    auto [rBuffer, message] =
        bsa_extract_file_data_by_filename(bsa.get(), texturePath_utf16);
    if (message.code == BSA_RESULT_EXCEPTION) {
      continue;
    }

    auto buffer_free = [&bsa](const bsa_result_buffer_t* buffer) {
      bsa_file_data_free(bsa.get(), *buffer);
    };
    using buffer_ptr  = std::unique_ptr<bsa_result_buffer_t, decltype(buffer_free)>;
    const auto buffer = buffer_ptr(&rBuffer, buffer_free);

    const auto data = static_cast<char*>(buffer->data);
    if (const auto texture = makeTexture(gli::load(data, buffer->size))) {
      return texture;
    }
  }

  return nullptr;
}

QOpenGLTexture* TextureManager::makeTexture(const gli::texture& texture)
{
  if (texture.empty()) {
    return nullptr;
  }

  const gli::gl GL(gli::gl::PROFILE_GL32);
  const auto [Internal, External, Type, Swizzles] =
      GL.translate(texture.format(), texture.swizzles());
  GLenum target = GL.translate(texture.target());

  const auto f = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_2_1>(
      QOpenGLContext::currentContext());
  auto* glTexture = new QOpenGLTexture(static_cast<QOpenGLTexture::Target>(target));

  glTexture->create();
  glTexture->bind();
  glTexture->setMipLevels(static_cast<int>(texture.levels()));
  glTexture->setMipBaseLevel(0);
  glTexture->setMipMaxLevel(static_cast<int>(texture.levels()) - 1);
  glTexture->setMinMagFilters(QOpenGLTexture::LinearMipMapLinear,
                              QOpenGLTexture::Linear);
  glTexture->setSwizzleMask(static_cast<QOpenGLTexture::SwizzleValue>(Swizzles[0]),
                            static_cast<QOpenGLTexture::SwizzleValue>(Swizzles[1]),
                            static_cast<QOpenGLTexture::SwizzleValue>(Swizzles[2]),
                            static_cast<QOpenGLTexture::SwizzleValue>(Swizzles[3]));

  glTexture->setWrapMode(QOpenGLTexture::Repeat);

  const auto extent = texture.extent();

  glTexture->setSize(extent.x, extent.y, extent.z);
  glTexture->setFormat(static_cast<QOpenGLTexture::TextureFormat>(Internal));
  glTexture->allocateStorage(static_cast<QOpenGLTexture::PixelFormat>(External),
                             static_cast<QOpenGLTexture::PixelType>(Type));

  for (std::size_t layer = 0; layer < texture.layers(); layer++)
    for (std::size_t face = 0; face < texture.faces(); face++)
      for (std::size_t level = 0; level < texture.levels(); level++) {
        const auto extent3d = texture.extent(level);

        target = gli::is_target_cube(texture.target())
                     ? static_cast<GLenum>(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face)
                     : target;

        // Qt's upload functions lag badly so we just use the GL API
        // ReSharper disable once CppIncompleteSwitchStatement
        // ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
        switch (texture.target()) {
        case gli::TARGET_1D:
          if (is_compressed(texture.format())) {
            f->glCompressedTexSubImage1D(target, static_cast<int>(level), 0, extent3d.x,
                                         Internal,
                                         static_cast<int>(texture.size(level)),
                                         texture.data(layer, face, level));
          } else {
            f->glTexSubImage1D(target, static_cast<int>(level), 0, extent3d.x, External,
                               Type, texture.data(layer, face, level));
          }
          break;
        case gli::TARGET_1D_ARRAY:
        case gli::TARGET_2D:
        case gli::TARGET_CUBE:
          if (is_compressed(texture.format())) {
            f->glCompressedTexSubImage2D(
                target, static_cast<int>(level), 0, 0, extent3d.x,
                texture.target() == gli::TARGET_1D_ARRAY ? static_cast<int>(layer)
                                                         : extent3d.y,
                Internal, static_cast<int>(texture.size(level)),
                texture.data(layer, face, level));
          } else {
            f->glTexSubImage2D(target, static_cast<int>(level), 0, 0, extent3d.x,
                               texture.target() == gli::TARGET_1D_ARRAY
                                   ? static_cast<int>(layer)
                                   : extent3d.y,
                               External, Type, texture.data(layer, face, level));
          }
          break;
        case gli::TARGET_2D_ARRAY:
        case gli::TARGET_3D:
        case gli::TARGET_CUBE_ARRAY:
          if (is_compressed(texture.format())) {
            f->glCompressedTexSubImage3D(
                target, static_cast<int>(level), 0, 0, 0, extent3d.x, extent3d.y,
                texture.target() == gli::TARGET_3D ? extent3d.z
                                                   : static_cast<int>(layer),
                Internal, static_cast<int>(texture.size(level)),
                texture.data(layer, face, level));
          } else {
            f->glTexSubImage3D(
                target, static_cast<int>(level), 0, 0, 0, extent3d.x, extent3d.y,
                texture.target() == gli::TARGET_3D ? extent3d.z
                                                   : static_cast<int>(layer),
                External, Type, texture.data(layer, face, level));
          }
          break;
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
  const auto dataDir = game->dataDirectory();

  if (auto realPath = m_MOInfo->resolvePath(path); !realPath.isEmpty()) {
    return realPath;
  }

  auto dataPath = dataDir.absoluteFilePath(QDir::cleanPath(path));
  dataPath.replace('/', QDir::separator());

  if (QFileInfo::exists(dataPath)) {
    return dataPath;
  }

  return "";
}
