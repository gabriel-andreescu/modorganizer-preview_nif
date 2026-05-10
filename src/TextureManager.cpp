#include "TextureManager.h"
#include "PreviewNif.h"

#include <uibase/game_features/dataarchives.h>
#include <uibase/game_features/igamefeatures.h>
#include <uibase/ifiletree.h>
#include <uibase/iplugingame.h>

#include <gli/gli.hpp>
#include <gli/load_dds.hpp>

#if __has_include(<libbsarch/libbsarch.h>)
#include <libbsarch/libbsarch.h>
#else
#include <libbsarch.h>
#endif

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QOpenGLContext>
#include <QOpenGLFunctions_2_1>
#include <QOpenGLVersionFunctionsFactory>
#include <QVector4D>
#include <QtGui/qopenglext.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <exception>
#include <limits>
#include <memory>
#include <ranges>
#include <utility>

namespace {

constexpr std::size_t maxGliTextureLevels = 16;

using DdsHeader = gli::detail::dds_header;
using DdsHeader10 = gli::detail::dds_header10;
using DdsPixelFormat = gli::detail::dds_pixel_format;

struct LegacyDdsFormatCandidate {
    std::uint32_t bitsPerPixel;
    gli::format format;
    glm::u32vec4 mask;
};

const MOBase::IProfile* profilePointer(const MOBase::IProfile* profile) {
    return profile;
}

template <class Profile>
const MOBase::IProfile* profilePointer(const std::shared_ptr<Profile>& profile) {
    return profile.get();
}

const MOBase::IProfile* currentProfile(MOBase::IOrganizer* organizer) {
    if (!organizer) {
        return nullptr;
    }

    return profilePointer(organizer->profile());
}

bool sameMask(const glm::u32vec4& left, const glm::u32vec4& right) {
    return glm::all(glm::equal(left, right));
}

std::array<LegacyDdsFormatCandidate, 25> legacyDdsFormatCandidates(const gli::dx& dx) {
    return {{
        {.bitsPerPixel = 8,
            .format = gli::FORMAT_RG4_UNORM_PACK8,
            .mask = dx.translate(gli::FORMAT_RG4_UNORM_PACK8).Mask},
        {.bitsPerPixel = 8,
            .format = gli::FORMAT_L8_UNORM_PACK8,
            .mask = dx.translate(gli::FORMAT_L8_UNORM_PACK8).Mask},
        {.bitsPerPixel = 8,
            .format = gli::FORMAT_A8_UNORM_PACK8,
            .mask = dx.translate(gli::FORMAT_A8_UNORM_PACK8).Mask},
        {.bitsPerPixel = 8,
            .format = gli::FORMAT_R8_UNORM_PACK8,
            .mask = dx.translate(gli::FORMAT_R8_UNORM_PACK8).Mask},
        {.bitsPerPixel = 8,
            .format = gli::FORMAT_RG3B2_UNORM_PACK8,
            .mask = dx.translate(gli::FORMAT_RG3B2_UNORM_PACK8).Mask},
        {.bitsPerPixel = 16,
            .format = gli::FORMAT_RGBA4_UNORM_PACK16,
            .mask = dx.translate(gli::FORMAT_RGBA4_UNORM_PACK16).Mask},
        {.bitsPerPixel = 16,
            .format = gli::FORMAT_BGRA4_UNORM_PACK16,
            .mask = dx.translate(gli::FORMAT_BGRA4_UNORM_PACK16).Mask},
        {.bitsPerPixel = 16,
            .format = gli::FORMAT_R5G6B5_UNORM_PACK16,
            .mask = dx.translate(gli::FORMAT_R5G6B5_UNORM_PACK16).Mask},
        {.bitsPerPixel = 16,
            .format = gli::FORMAT_B5G6R5_UNORM_PACK16,
            .mask = dx.translate(gli::FORMAT_B5G6R5_UNORM_PACK16).Mask},
        {.bitsPerPixel = 16,
            .format = gli::FORMAT_RGB5A1_UNORM_PACK16,
            .mask = dx.translate(gli::FORMAT_RGB5A1_UNORM_PACK16).Mask},
        {.bitsPerPixel = 16,
            .format = gli::FORMAT_BGR5A1_UNORM_PACK16,
            .mask = dx.translate(gli::FORMAT_BGR5A1_UNORM_PACK16).Mask},
        {.bitsPerPixel = 16,
            .format = gli::FORMAT_LA8_UNORM_PACK8,
            .mask = dx.translate(gli::FORMAT_LA8_UNORM_PACK8).Mask},
        {.bitsPerPixel = 16,
            .format = gli::FORMAT_RG8_UNORM_PACK8,
            .mask = dx.translate(gli::FORMAT_RG8_UNORM_PACK8).Mask},
        {.bitsPerPixel = 16,
            .format = gli::FORMAT_L16_UNORM_PACK16,
            .mask = dx.translate(gli::FORMAT_L16_UNORM_PACK16).Mask},
        {.bitsPerPixel = 16,
            .format = gli::FORMAT_A16_UNORM_PACK16,
            .mask = dx.translate(gli::FORMAT_A16_UNORM_PACK16).Mask},
        {.bitsPerPixel = 16,
            .format = gli::FORMAT_R16_UNORM_PACK16,
            .mask = dx.translate(gli::FORMAT_R16_UNORM_PACK16).Mask},
        {.bitsPerPixel = 24,
            .format = gli::FORMAT_RGB8_UNORM_PACK8,
            .mask = dx.translate(gli::FORMAT_RGB8_UNORM_PACK8).Mask},
        {.bitsPerPixel = 24,
            .format = gli::FORMAT_BGR8_UNORM_PACK8,
            .mask = dx.translate(gli::FORMAT_BGR8_UNORM_PACK8).Mask},
        {.bitsPerPixel = 32,
            .format = gli::FORMAT_BGR8_UNORM_PACK32,
            .mask = dx.translate(gli::FORMAT_BGR8_UNORM_PACK32).Mask},
        {.bitsPerPixel = 32,
            .format = gli::FORMAT_BGRA8_UNORM_PACK8,
            .mask = dx.translate(gli::FORMAT_BGRA8_UNORM_PACK8).Mask},
        {.bitsPerPixel = 32,
            .format = gli::FORMAT_RGBA8_UNORM_PACK8,
            .mask = dx.translate(gli::FORMAT_RGBA8_UNORM_PACK8).Mask},
        {.bitsPerPixel = 32,
            .format = gli::FORMAT_RGB10A2_UNORM_PACK32,
            .mask = dx.translate(gli::FORMAT_RGB10A2_UNORM_PACK32).Mask},
        {.bitsPerPixel = 32,
            .format = gli::FORMAT_LA16_UNORM_PACK16,
            .mask = dx.translate(gli::FORMAT_LA16_UNORM_PACK16).Mask},
        {.bitsPerPixel = 32,
            .format = gli::FORMAT_RG16_UNORM_PACK16,
            .mask = dx.translate(gli::FORMAT_RG16_UNORM_PACK16).Mask},
        {.bitsPerPixel = 32,
            .format = gli::FORMAT_R32_SFLOAT_PACK32,
            .mask = dx.translate(gli::FORMAT_R32_SFLOAT_PACK32).Mask},
    }};
}

bool isArchiveName(const QString& name) {
    return name.endsWith(".bsa", Qt::CaseInsensitive) || name.endsWith(".ba2", Qt::CaseInsensitive);
}

bool checkedAdd(const std::size_t left, const std::size_t right, std::size_t& result) {
    if (left > std::numeric_limits<std::size_t>::max() - right) {
        return false;
    }

    result = left + right;
    return true;
}

bool checkedMul(const std::size_t left, const std::size_t right, std::size_t& result) {
    if (left != 0 && right > std::numeric_limits<std::size_t>::max() / left) {
        return false;
    }

    result = left * right;
    return true;
}

std::size_t mipExtent(const std::size_t extent, const std::size_t level) {
    return std::max<std::size_t>(extent >> level, 1);
}

bool ddsPayloadSize(
    const gli::format format,
    const std::size_t width,
    const std::size_t height,
    const std::size_t depth,
    const std::size_t faces,
    const std::size_t levels,
    std::size_t& payloadSize
) {
    const auto blockSize = gli::block_size(format);
    const auto blockExtent = gli::block_extent(format);

    if (blockSize
        == 0
        || blockExtent.x
        <= 0
        || blockExtent.y
        <= 0
        || blockExtent.z
        <= 0
        || width
        == 0
        || height
        == 0
        || depth
        == 0
        || faces
        == 0
        || levels
        == 0
        || levels
        > maxGliTextureLevels) {
        return false;
    }

    payloadSize = 0;

    for (std::size_t level = 0; level < levels; ++level) {
        const auto levelWidth = mipExtent(width, level);
        const auto levelHeight = mipExtent(height, level);
        const auto levelDepth = mipExtent(depth, level);

        const auto blocksX = (levelWidth + static_cast<std::size_t>(blockExtent.x) - 1)
                             / static_cast<std::size_t>(blockExtent.x);
        const auto blocksY = (levelHeight + static_cast<std::size_t>(blockExtent.y) - 1)
                             / static_cast<std::size_t>(blockExtent.y);
        const auto blocksZ = (levelDepth + static_cast<std::size_t>(blockExtent.z) - 1)
                             / static_cast<std::size_t>(blockExtent.z);

        std::size_t levelBlocks = 0;
        if (!checkedMul(blocksX, blocksY, levelBlocks)
            || !checkedMul(levelBlocks, blocksZ, levelBlocks)
            || !checkedMul(levelBlocks, blockSize, levelBlocks)
            || !checkedAdd(payloadSize, levelBlocks, payloadSize)) {
            return false;
        }
    }

    return checkedMul(payloadSize, faces, payloadSize);
}

bool hasLegacyPixelFormat(const DdsPixelFormat& format) {
    return (format.flags
               & (gli::dx::DDPF_RGB
                   | gli::dx::DDPF_ALPHAPIXELS
                   | gli::dx::DDPF_ALPHA
                   | gli::dx::DDPF_YUV
                   | gli::dx::DDPF_LUMINANCE))
           && format.bpp
           != 0
           && format.bpp
           < 64;
}

gli::format legacyDdsFormat(const DdsHeader& header) {
    gli::dx dx;

    if (!hasLegacyPixelFormat(header.Format)) {
        return gli::FORMAT_UNDEFINED;
    }

    for (const auto& candidate : legacyDdsFormatCandidates(dx)) {
        if (candidate.bitsPerPixel == header.Format.bpp && sameMask(header.Format.Mask, candidate.mask)) {
            return candidate.format;
        }
    }

    return gli::FORMAT_UNDEFINED;
}

DdsHeader10 emptyDdsHeader10() {
    DdsHeader10 header;
    std::memset(&header, 0, sizeof(header));
    return header;
}

DdsHeader emptyDdsHeader() {
    DdsHeader header;
    std::memset(&header, 0, sizeof(header));
    return header;
}

class DdsTextureReader {
public:
    DdsTextureReader(const char* data, const std::size_t size)
        : m_Data(data)
        , m_Size(size)
        , m_Header(emptyDdsHeader())
        , m_Header10(emptyDdsHeader10()) {}

    gli::texture load() {
        if (!readHeader()) {
            return {};
        }

        const auto format = resolveFormat();
        if (format == gli::FORMAT_UNDEFINED) {
            return {};
        }

        const auto target = gli::detail::get_target(m_Header, m_Header10);
        if (!isSupportedTarget(target)) {
            return {};
        }

        const auto mipMapCount = this->mipMapCount();
        if (mipMapCount == 0) {
            return {};
        }

        const auto faceCount = this->faceCount();
        if (!hasValidFaceCount(target, faceCount)) {
            return {};
        }

        const auto layerCount = this->layerCount();
        if (layerCount != 1) {
            return {};
        }

        const auto depthCount = this->depthCount();
        if (depthCount == 0) {
            return {};
        }

        const auto textureExtent = gli::texture::extent_type(m_Header.Width, m_Header.Height, depthCount);
        if (!hasValidMipCount(textureExtent, mipMapCount)
            || !hasValidPayload(format, depthCount, faceCount, mipMapCount)) {
            return {};
        }

        gli::texture texture(target, format, textureExtent, layerCount, faceCount, mipMapCount);
        if (!canCopyTexture(texture)) {
            return {};
        }

        std::memcpy(texture.data(), m_Data + m_Offset, texture.size());
        return texture;
    }

private:
    bool readHeader() {
        if (!hasBaseHeader()) {
            return false;
        }

        m_Offset = sizeof(gli::detail::FOURCC_DDS);
        std::memcpy(&m_Header, m_Data + m_Offset, sizeof(m_Header));
        m_Offset += sizeof(DdsHeader);

        if (!hasValidHeader()) {
            return false;
        }

        return !hasDx10Header() || readDx10Header();
    }

    [[nodiscard]] bool hasBaseHeader() const {
        return m_Data
               && m_Size
               >= sizeof(gli::detail::FOURCC_DDS)
               + sizeof(DdsHeader)
               && std::strncmp(m_Data, gli::detail::FOURCC_DDS, sizeof(gli::detail::FOURCC_DDS))
               == 0;
    }

    [[nodiscard]] bool hasValidHeader() const {
        return m_Header.Size
               == sizeof(DdsHeader)
               && m_Header.Format.size
               == sizeof(DdsPixelFormat)
               && m_Header.Width
               != 0
               && m_Header.Height
               != 0;
    }

    [[nodiscard]] bool hasFourCc() const {
        return (m_Header.Format.flags & gli::dx::DDPF_FOURCC) != 0;
    }

    [[nodiscard]] bool hasDx10Header() const {
        return hasFourCc()
               && (m_Header.Format.fourCC == gli::dx::D3DFMT_DX10 || m_Header.Format.fourCC == gli::dx::D3DFMT_GLI1);
    }

    bool readDx10Header() {
        if (m_Size < m_Offset + sizeof(DdsHeader10)) {
            return false;
        }

        std::memcpy(&m_Header10, m_Data + m_Offset, sizeof(m_Header10));
        m_Offset += sizeof(DdsHeader10);
        return true;
    }

    [[nodiscard]] gli::format resolveFormat() const {
        gli::dx dxTranslator;
        auto format = legacyDdsFormat(m_Header);

        if (hasDx10Header()) {
            return dxTranslator.find(m_Header.Format.fourCC, m_Header10.Format);
        }

        if (hasFourCc() && format == gli::FORMAT_UNDEFINED) {
            return dxTranslator.find(gli::detail::remap_four_cc(m_Header.Format.fourCC));
        }

        return format;
    }

    [[nodiscard]] static bool isSupportedTarget(const gli::target target) {
        return target == gli::TARGET_2D || target == gli::TARGET_CUBE;
    }

    [[nodiscard]] std::size_t mipMapCount() const {
        return (m_Header.Flags & gli::detail::DDSD_MIPMAPCOUNT) ? m_Header.MipMapLevels : 1;
    }

    [[nodiscard]] std::size_t faceCount() const {
        if (m_Header.CubemapFlags & gli::detail::DDSCAPS2_CUBEMAP) {
            return static_cast<std::size_t>(
                glm::bitCount(m_Header.CubemapFlags & gli::detail::DDSCAPS2_CUBEMAP_ALLFACES)
            );
        }

        if (m_Header10.MiscFlag & gli::detail::D3D10_RESOURCE_MISC_TEXTURECUBE) {
            return 6;
        }

        return 1;
    }

    [[nodiscard]] static bool hasValidFaceCount(const gli::target target, const std::size_t faceCount) {
        return (target == gli::TARGET_2D && faceCount == 1) || (target == gli::TARGET_CUBE && faceCount == 6);
    }

    [[nodiscard]] std::size_t layerCount() const {
        return std::max<gli::texture::size_type>(m_Header10.ArraySize, 1);
    }

    [[nodiscard]] std::size_t depthCount() const {
        return (m_Header.CubemapFlags & gli::detail::DDSCAPS2_VOLUME) ? m_Header.Depth : 1;
    }

    [[nodiscard]] static bool hasValidMipCount(
        const gli::texture::extent_type textureExtent,
        const std::size_t mipMapCount
    ) {
        return mipMapCount <= std::min<std::size_t>(gli::levels(textureExtent), maxGliTextureLevels);
    }

    [[nodiscard]] bool hasValidPayload(
        const gli::format format,
        const std::size_t depthCount,
        const std::size_t faceCount,
        const std::size_t mipMapCount
    ) const {
        std::size_t payloadSize = 0;
        return ddsPayloadSize(format, m_Header.Width, m_Header.Height, depthCount, faceCount, mipMapCount, payloadSize)
               && payloadSize
               <= m_Size
               - m_Offset;
    }

    [[nodiscard]] bool canCopyTexture(const gli::texture& texture) const {
        return !texture.empty() && texture.layers() == 1 && texture.size() <= m_Size - m_Offset;
    }

    const char* m_Data = nullptr;
    std::size_t m_Size = 0;
    std::size_t m_Offset = 0;
    DdsHeader m_Header;
    DdsHeader10 m_Header10;
};

gli::texture loadDdsIfValid(const char* data, const std::size_t size) {
    DdsTextureReader reader(data, size);
    return reader.load();
}

gli::texture loadDdsFileIfValid(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    const auto data = file.readAll();
    return loadDdsIfValid(data.constData(), static_cast<std::size_t>(data.size()));
}

bool hasUploadableExtents(const gli::texture& texture) {
    if (texture.levels() == 0 || texture.layers() != 1 || texture.faces() == 0) {
        return false;
    }

    if (texture.target() == gli::TARGET_2D && texture.faces() != 1) {
        return false;
    }
    if (texture.target() == gli::TARGET_CUBE && texture.faces() != 6) {
        return false;
    }

    for (std::size_t level = 0; level < texture.levels(); ++level) {
        const auto extent = texture.extent(level);
        if (extent.x
            <= 0
            || extent.y
            <= 0
            || extent.z
            <= 0
            || extent.x
            > std::numeric_limits<GLsizei>::max()
            || extent.y
            > std::numeric_limits<GLsizei>::max()
            || texture.size(level)
            > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max())) {
            return false;
        }
    }

    return texture.levels() <= static_cast<std::size_t>(std::numeric_limits<GLsizei>::max());
}

PFNGLTEXSTORAGE2DPROC resolveTexStorage2D(const QOpenGLContext* context) {
    return context ? reinterpret_cast<PFNGLTEXSTORAGE2DPROC>(context->getProcAddress("glTexStorage2D")) : nullptr;
}

void clearGlErrors(QOpenGLFunctions_2_1* f) {
    while (f->glGetError() != GL_NO_ERROR) {}
}

GLenum textureFaceTarget(const gli::texture& texture, const GLenum target, const std::size_t face) {
    return gli::is_target_cube(texture.target()) ? static_cast<GLenum>(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face) : target;
}

GLenum textureUploadTarget(const gli::target target) {
    if (target == gli::TARGET_2D) {
        return GL_TEXTURE_2D;
    }

    if (target == gli::TARGET_CUBE) {
        return GL_TEXTURE_CUBE_MAP;
    }

    return 0;
}

void setTextureParameters(
    QOpenGLFunctions_2_1* f,
    const GLenum target,
    const gli::gl::format& format,
    const std::size_t levels
) {
    f->glTexParameteri(target, GL_TEXTURE_BASE_LEVEL, 0);
    f->glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(levels - 1));
    f->glTexParameteri(target, GL_TEXTURE_MIN_FILTER, levels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    f->glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    f->glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_REPEAT);
    f->glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_REPEAT);
    f->glTexParameteri(target, static_cast<GLenum>(QOpenGLTexture::SwizzleRed), format.Swizzles[0]);
    f->glTexParameteri(target, static_cast<GLenum>(QOpenGLTexture::SwizzleGreen), format.Swizzles[1]);
    f->glTexParameteri(target, static_cast<GLenum>(QOpenGLTexture::SwizzleBlue), format.Swizzles[2]);
    f->glTexParameteri(target, static_cast<GLenum>(QOpenGLTexture::SwizzleAlpha), format.Swizzles[3]);
}

GLenum uploadTextureData(
    const gli::texture& texture,
    QOpenGLFunctions_2_1* f,
    PFNGLTEXSTORAGE2DPROC glTexStorage2D,
    const GLenum target,
    const gli::gl::format& format,
    const bool useStorage
) {
    if (useStorage) {
        const auto extent = texture.extent();
        glTexStorage2D(target, static_cast<GLsizei>(texture.levels()), format.Internal, extent.x, extent.y);
    }

    for (std::size_t face = 0; face < texture.faces(); ++face) {
        for (std::size_t level = 0; level < texture.levels(); ++level) {
            const auto extent = texture.extent(level);
            const auto targetFace = textureFaceTarget(texture, target, face);
            const auto* textureData = texture.data(0, face, level);
            const auto textureSize = static_cast<GLsizei>(texture.size(level));

            if (gli::is_compressed(texture.format())) {
                if (useStorage) {
                    f->glCompressedTexSubImage2D(
                        targetFace,
                        static_cast<GLint>(level),
                        0,
                        0,
                        extent.x,
                        extent.y,
                        format.Internal,
                        textureSize,
                        textureData
                    );
                } else {
                    f->glCompressedTexImage2D(
                        targetFace,
                        static_cast<GLint>(level),
                        format.Internal,
                        extent.x,
                        extent.y,
                        0,
                        textureSize,
                        textureData
                    );
                }
            } else if (useStorage) {
                f->glTexSubImage2D(
                    targetFace,
                    static_cast<GLint>(level),
                    0,
                    0,
                    extent.x,
                    extent.y,
                    format.External,
                    format.Type,
                    textureData
                );
            } else {
                f->glTexImage2D(
                    targetFace,
                    static_cast<GLint>(level),
                    format.Internal,
                    extent.x,
                    extent.y,
                    0,
                    format.External,
                    format.Type,
                    textureData
                );
            }
        }
    }

    return f->glGetError();
}

GLuint makeRawTexture(
    const gli::texture& texture,
    QOpenGLFunctions_2_1* f,
    const GLenum target,
    const gli::gl::format& format,
    PFNGLTEXSTORAGE2DPROC glTexStorage2D
) {
    for (const bool useStorage : {true, false}) {
        if (useStorage && !glTexStorage2D) {
            continue;
        }

        GLuint textureId = 0;
        f->glGenTextures(1, &textureId);
        if (textureId == 0) {
            continue;
        }

        f->glBindTexture(target, textureId);
        setTextureParameters(f, target, format, texture.levels());
        clearGlErrors(f);
        f->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        const auto error = uploadTextureData(texture, f, glTexStorage2D, target, format, useStorage);

        f->glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        f->glBindTexture(target, 0);

        if (error == GL_NO_ERROR) {
            return textureId;
        }

        qWarning(
            "Failed to upload DDS texture with %s storage: OpenGL error 0x%x",
            useStorage ? "immutable" : "mutable",
            error
        );
        f->glDeleteTextures(1, &textureId);
    }

    return 0;
}

} // namespace

struct BsaPtrDeleter {
    void operator()(void* ptr) const {
        bsa_free(ptr);
    }
};

using UniqueBsaPtr = std::unique_ptr<void, BsaPtrDeleter>;

struct BsaBufferDeleter {
    explicit BsaBufferDeleter(void* bsa)
        : m_bsa(bsa) {}

    void operator()(const bsa_result_buffer_t* buffer) const {
        bsa_file_data_free(m_bsa, *buffer);
    }

    void* m_bsa;
};

using UniqueBufferPtr = std::unique_ptr<bsa_result_buffer_t, BsaBufferDeleter>;

PreviewTexture::PreviewTexture(QOpenGLTexture* texture)
    : m_QtTexture(texture) {}

PreviewTexture::PreviewTexture(const GLuint textureId, const GLenum target)
    : m_TextureId(textureId)
    , m_Target(target) {}

PreviewTexture::~PreviewTexture() {
    delete m_QtTexture;
    m_QtTexture = nullptr;

    if (m_TextureId == 0) {
        return;
    }

    auto* context = QOpenGLContext::currentContext();
    if (!context) {
        qWarning("Leaking raw DDS OpenGL texture %u: no current context", m_TextureId);
        return;
    }

    if (auto* f = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_2_1>(context)) {
        f->glDeleteTextures(1, &m_TextureId);
    } else {
        qWarning("Leaking raw DDS OpenGL texture %u: OpenGL 2.1 functions unavailable", m_TextureId);
        return;
    }

    m_TextureId = 0;
}

void PreviewTexture::bind(const int textureUnit) const {
    if (m_QtTexture) {
        m_QtTexture->bind(textureUnit);
        return;
    }

    if (m_TextureId == 0) {
        return;
    }

    if (auto* f = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_2_1>(QOpenGLContext::currentContext())) {
        f->glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(textureUnit));
        f->glBindTexture(m_Target, m_TextureId);
    }
}

TextureManager::TextureManager(MOBase::IOrganizer* organizer, TextureSourceProvider textureSource)
    : m_MOInfo {organizer}
    , m_TextureSource {std::move(textureSource)} {}

void TextureManager::cleanup() {
    for (auto it = m_Textures.cbegin(); it != m_Textures.cend();) {
        const auto* texture = it->second;
        m_Textures.erase(it++);
        delete texture;
    }

    auto cleanupTexture = [&](PreviewTexture*& texPtr) {
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

PreviewTexture* TextureManager::getTexture(const std::string& texturePath) {
    return getTexture(QString::fromStdString(texturePath));
}

PreviewTexture* TextureManager::getTexture(const QString& texturePath) {
    if (texturePath.isEmpty()) {
        return nullptr;
    }

    const auto key = texturePath.toLower().toStdWString();

    if (const auto it = m_Textures.find(key); it != m_Textures.end()) {
        return it->second;
    }

    PreviewTexture* texture = nullptr;
    try {
        texture = loadTexture(texturePath);
    } catch (const std::exception& e) {
        qWarning("Failed to load NIF texture '%s': %s", qUtf8Printable(texturePath), e.what());
    } catch (...) {
        qWarning("Failed to load NIF texture '%s': unknown exception", qUtf8Printable(texturePath));
    }

    m_Textures[key] = texture;
    return texture;
}

PreviewTexture* TextureManager::getErrorTexture() {
    if (!m_ErrorTexture) {
        m_ErrorTexture = makeSolidColor({1.0f, 0.0f, 1.0f, 1.0f});
    }
    return m_ErrorTexture;
}

PreviewTexture* TextureManager::getBlackTexture() {
    if (!m_BlackTexture) {
        m_BlackTexture = makeSolidColor({0.0f, 0.0f, 0.0f, 1.0f});
    }
    return m_BlackTexture;
}

PreviewTexture* TextureManager::getWhiteTexture() {
    if (!m_WhiteTexture) {
        m_WhiteTexture = makeSolidColor({1.0f, 1.0f, 1.0f, 1.0f});
    }
    return m_WhiteTexture;
}

PreviewTexture* TextureManager::getFlatNormalTexture() {
    if (!m_FlatNormalTexture) {
        m_FlatNormalTexture = makeSolidColor({0.5f, 0.5f, 1.0f, 1.0f});
    }
    return m_FlatNormalTexture;
}

PreviewTexture* TextureManager::loadTexture(const QString& texturePath) const {
    if (auto* const texture = tryLoadTextureFromSource(texturePath)) {
        return texture;
    }

    return loadTextureAuto(texturePath);
}

PreviewTexture* TextureManager::loadTextureAuto(const QString& texturePath) const {
    if (texturePath.isEmpty()) {
        return nullptr;
    }

    if (!m_MOInfo) {
        qCritical("Failed to interface with Mod Organizer");
        return nullptr;
    }

    const auto* const game = m_MOInfo->managedGame();
    if (!game) {
        qCritical("Failed to interface with managed game plugin");
        return nullptr;
    }

    const auto realPath = resolvePath(game, texturePath);
    const bool fileExists = !realPath.isEmpty() && QFileInfo::exists(realPath) && QFileInfo(realPath).isFile();

    if (fileExists) {
        try {
            auto texture = loadDdsFileIfValid(realPath);
            if (texture.empty()) {
                qWarning("Failed to decode loose DDS '%s': invalid or unsupported DDS", qUtf8Printable(realPath));
                return nullptr;
            }
            return makeTexture(texture);
        } catch (const std::exception& e) {
            qWarning("Failed to decode loose DDS '%s': %s", qUtf8Printable(realPath), e.what());
            return nullptr;
        }
    }

    if (auto* const texture = tryLoadTextureFromMods(texturePath)) {
        return texture;
    }

    if (auto* const texture = tryLoadTextureFromGame(texturePath)) {
        return texture;
    }

    return nullptr;
}

PreviewTexture* TextureManager::tryLoadTextureFromSource(const QString& texturePath) const {
    if (m_TextureSource.kind
        == TextureSourceProviderKind::Auto
        || !textureProviderCoversPath(m_TextureSource, texturePath)) {
        return nullptr;
    }

    switch (m_TextureSource.kind) {
        case TextureSourceProviderKind::Mod:
        case TextureSourceProviderKind::GameData: {
            if (!m_TextureSource.sourcePath.isEmpty()) {
                const auto realPath = QDir(m_TextureSource.sourcePath)
                                          .absoluteFilePath(QDir::cleanPath(normalizeTextureDataPath(texturePath)));
                if (QFileInfo::exists(realPath) && QFileInfo(realPath).isFile()) {
                    if (auto* const texture = loadLooseTexture(realPath)) {
                        return texture;
                    }
                }
            }

            return tryLoadTextureFromArchives(m_TextureSource.archivePaths, texturePath);
        }
        case TextureSourceProviderKind::Auto: return nullptr;
    }

    return nullptr;
}

PreviewTexture* TextureManager::loadLooseTexture(const QString& path) {
    try {
        auto texture = loadDdsFileIfValid(path);
        if (texture.empty()) {
            qWarning("Failed to decode loose DDS '%s': invalid or unsupported DDS", qUtf8Printable(path));
            return nullptr;
        }
        return makeTexture(texture);
    } catch (const std::exception& e) {
        qWarning("Failed to decode loose DDS '%s': %s", qUtf8Printable(path), e.what());
        return nullptr;
    }
}

PreviewTexture* TextureManager::tryLoadTextureFromArchives(
    const QStringList& archivePaths,
    const QString& texturePath
) {
    for (const auto& archivePath : archivePaths) {
        if (auto* const texture = loadTextureFromBSA(archivePath, texturePath)) {
            return texture;
        }
    }

    return nullptr;
}

PreviewTexture* TextureManager::tryLoadTextureFromMods(const QString& texturePath) const {
    if (!m_MOInfo) {
        return nullptr;
    }

    const auto fileOrigins = m_MOInfo->getFileOrigins(texturePath);
    if (fileOrigins.empty()) {
        return nullptr;
    }

    const auto& modName = fileOrigins.constFirst();
    if (auto* const mod = m_MOInfo->modList()->getMod(modName)) {
        if (const auto fileTree = mod->fileTree()) {
            for (auto it = fileTree->begin(); it != fileTree->end(); ++it) {
                const auto fileInfo = *it;
                if (!fileInfo) {
                    continue;
                }
                if (!isArchiveName(fileInfo->name())) {
                    continue;
                }

                const auto bsaPath = resolvePath(m_MOInfo->managedGame(), fileInfo->name());
                if (bsaPath.isEmpty()) {
                    continue;
                }
                if (auto* const texture = loadTextureFromBSA(bsaPath, texturePath)) {
                    return texture;
                }
            }
        }
    }
    return nullptr;
}

PreviewTexture* TextureManager::tryLoadTextureFromGame(const QString& texturePath) const {
    if (!m_MOInfo) {
        return nullptr;
    }

    auto* const features = m_MOInfo->gameFeatures();
    if (!features) {
        return nullptr;
    }

    const auto gameArchives = features->gameFeature<MOBase::DataArchives>();
    if (!gameArchives) {
        return nullptr;
    }

    for (
        auto archives = gameArchives->archives(currentProfile(m_MOInfo));
        const auto& archive : std::ranges::reverse_view(archives)
    ) {
        const auto bsaPath = resolvePath(m_MOInfo->managedGame(), archive);
        if (bsaPath.isEmpty()) {
            continue;
        }
        if (auto* const texture = loadTextureFromBSA(bsaPath, texturePath)) {
            return texture;
        }
    }
    return nullptr;
}

PreviewTexture* TextureManager::loadTextureFromBSA(const QString& bsaPath, const QString& texturePath) {
    const UniqueBsaPtr bsaHandle(bsa_create());
    if (!bsaHandle) {
        qWarning("Failed to create BSA handle while loading '%s'", qUtf8Printable(texturePath));
        return nullptr;
    }

    static_assert(sizeof(wchar_t) == 2, "Expected wchar_t to be 2 bytes");

    const auto* const bsaPathUtf16 = reinterpret_cast<const wchar_t*>(bsaPath.utf16());
    const auto [code, _text] = bsa_load_from_file(bsaHandle.get(), bsaPathUtf16);
    if (code == BSA_RESULT_EXCEPTION) {
        return nullptr;
    }

    const auto archiveTexturePath = QDir::toNativeSeparators(texturePath);
    const auto* const texturePathUtf16 = reinterpret_cast<const wchar_t*>(archiveTexturePath.utf16());
    auto [rBuffer, msg] = bsa_extract_file_data_by_filename(bsaHandle.get(), texturePathUtf16);
    if (msg.code == BSA_RESULT_EXCEPTION) {
        return nullptr;
    }

    if (!rBuffer.data || rBuffer.size == 0) {
        return nullptr;
    }

    const UniqueBufferPtr buffer(&rBuffer, BsaBufferDeleter(bsaHandle.get()));

    const auto* const data = static_cast<const char*>(buffer->data);
    try {
        auto texture = loadDdsIfValid(data, static_cast<std::size_t>(buffer->size));
        if (texture.empty()) {
            qWarning(
                "Failed to decode BSA DDS '%s' from '%s': invalid or unsupported DDS",
                qUtf8Printable(texturePath),
                qUtf8Printable(bsaPath)
            );
            return nullptr;
        }
        return makeTexture(texture);
    } catch (const std::exception& e) {
        qWarning(
            "Failed to decode BSA DDS '%s' from '%s': %s",
            qUtf8Printable(texturePath),
            qUtf8Printable(bsaPath),
            e.what()
        );
        return nullptr;
    }
}

PreviewTexture* TextureManager::makeTexture(const gli::texture& texture) {
    if (texture.empty()) {
        return nullptr;
    }

    if (!hasUploadableExtents(texture)) {
        qWarning("Skipping DDS texture with invalid or unsupported image layout");
        return nullptr;
    }

    auto* context = QOpenGLContext::currentContext();
    auto* f = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_2_1>(context);
    if (!f) {
        qWarning("Skipping DDS texture: OpenGL 2.1 functions unavailable");
        return nullptr;
    }

    const gli::gl gl(gli::gl::PROFILE_GL33);
    const auto format = gl.translate(texture.format(), texture.swizzles());
    const auto target = textureUploadTarget(texture.target());
    if (target == 0) {
        qWarning("Skipping DDS texture with unsupported OpenGL texture target");
        return nullptr;
    }

    const auto textureId = makeRawTexture(texture, f, target, format, resolveTexStorage2D(context));
    if (textureId == 0) {
        qWarning("Skipping DDS texture after failed OpenGL upload");
        return nullptr;
    }

    return new PreviewTexture(textureId, target);
}

PreviewTexture* TextureManager::makeSolidColor(const QVector4D color) {
    auto* glTexture = new QOpenGLTexture(QOpenGLTexture::Target2D);
    glTexture->create();
    glTexture->bind();

    glTexture->setSize(1, 1);
    glTexture->setFormat(QOpenGLTexture::RGBA32F);
    glTexture->allocateStorage(QOpenGLTexture::RGBA, QOpenGLTexture::Float32);

    glTexture->setData(QOpenGLTexture::RGBA, QOpenGLTexture::Float32, &color);

    glTexture->release();
    return new PreviewTexture(glTexture);
}

QString TextureManager::resolvePath(const MOBase::IPluginGame* game, const QString& path) const {
    if (!m_MOInfo) {
        return {};
    }

    if (auto resolved = m_MOInfo->resolvePath(path); !resolved.isEmpty()) {
        return resolved;
    }

    if (!game) {
        return {};
    }

    const auto dataPath = game->dataDirectory().absoluteFilePath(QDir::cleanPath(path));

    return QFileInfo::exists(dataPath) ? dataPath : QString();
}
