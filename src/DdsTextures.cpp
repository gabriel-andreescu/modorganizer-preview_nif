#include "DdsTextures.h"

#include <gli/load_dds.hpp>

#include <QFile>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>

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

} // namespace

gli::texture DdsTextures::loadIfValid(const char* data, const std::size_t size) {
    DdsTextureReader reader(data, size);
    return reader.load();
}

gli::texture DdsTextures::loadFileIfValid(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    const auto data = file.readAll();
    return loadIfValid(data.constData(), static_cast<std::size_t>(data.size()));
}
