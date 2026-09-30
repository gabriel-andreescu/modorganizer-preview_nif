#include "Textures/Dds.h"

#include <gli/load_dds.hpp>

#include <QFile>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <span>

namespace {

constexpr std::size_t kMaxGliTextureLevels = 16;

using DdsHeader = gli::detail::dds_header;
using DdsHeader10 = gli::detail::dds_header10;
using DdsPixelFormat = gli::detail::dds_pixel_format;

// gli declares its DDS flags as signed enumerators.
template <typename Flag>
constexpr std::uint32_t FlagBits(const Flag a_flag) {
    return static_cast<std::uint32_t>(a_flag);
}

constexpr std::uint32_t kLegacyPixelFormatFlags = FlagBits(gli::dx::DDPF_RGB)
                                                  | FlagBits(gli::dx::DDPF_ALPHAPIXELS)
                                                  | FlagBits(gli::dx::DDPF_ALPHA)
                                                  | FlagBits(gli::dx::DDPF_YUV)
                                                  | FlagBits(gli::dx::DDPF_LUMINANCE);

struct LegacyDdsFormatCandidate {
    std::uint32_t bitsPerPixel;
    gli::format format;
};

// Candidates share bit depths, so the first whose channel masks match wins.
constexpr std::array<LegacyDdsFormatCandidate, 25> kLegacyDdsFormatCandidates {
    {
        {.bitsPerPixel = 8, .format = gli::FORMAT_RG4_UNORM_PACK8},
        {.bitsPerPixel = 8, .format = gli::FORMAT_L8_UNORM_PACK8},
        {.bitsPerPixel = 8, .format = gli::FORMAT_A8_UNORM_PACK8},
        {.bitsPerPixel = 8, .format = gli::FORMAT_R8_UNORM_PACK8},
        {.bitsPerPixel = 8, .format = gli::FORMAT_RG3B2_UNORM_PACK8},
        {.bitsPerPixel = 16, .format = gli::FORMAT_RGBA4_UNORM_PACK16},
        {.bitsPerPixel = 16, .format = gli::FORMAT_BGRA4_UNORM_PACK16},
        {.bitsPerPixel = 16, .format = gli::FORMAT_R5G6B5_UNORM_PACK16},
        {.bitsPerPixel = 16, .format = gli::FORMAT_B5G6R5_UNORM_PACK16},
        {.bitsPerPixel = 16, .format = gli::FORMAT_RGB5A1_UNORM_PACK16},
        {.bitsPerPixel = 16, .format = gli::FORMAT_BGR5A1_UNORM_PACK16},
        {.bitsPerPixel = 16, .format = gli::FORMAT_LA8_UNORM_PACK8},
        {.bitsPerPixel = 16, .format = gli::FORMAT_RG8_UNORM_PACK8},
        {.bitsPerPixel = 16, .format = gli::FORMAT_L16_UNORM_PACK16},
        {.bitsPerPixel = 16, .format = gli::FORMAT_A16_UNORM_PACK16},
        {.bitsPerPixel = 16, .format = gli::FORMAT_R16_UNORM_PACK16},
        {.bitsPerPixel = 24, .format = gli::FORMAT_RGB8_UNORM_PACK8},
        {.bitsPerPixel = 24, .format = gli::FORMAT_BGR8_UNORM_PACK8},
        {.bitsPerPixel = 32, .format = gli::FORMAT_BGR8_UNORM_PACK32},
        {.bitsPerPixel = 32, .format = gli::FORMAT_BGRA8_UNORM_PACK8},
        {.bitsPerPixel = 32, .format = gli::FORMAT_RGBA8_UNORM_PACK8},
        {.bitsPerPixel = 32, .format = gli::FORMAT_RGB10A2_UNORM_PACK32},
        {.bitsPerPixel = 32, .format = gli::FORMAT_LA16_UNORM_PACK16},
        {.bitsPerPixel = 32, .format = gli::FORMAT_RG16_UNORM_PACK16},
        {.bitsPerPixel = 32, .format = gli::FORMAT_R32_SFLOAT_PACK32},
    },
};

bool SameMask(const glm::u32vec4& a_left, const glm::u32vec4& a_right) {
    return glm::all(glm::equal(a_left, a_right));
}

bool CheckedAdd(const std::size_t a_left, const std::size_t a_right, std::size_t& a_result) {
    if (a_left > std::numeric_limits<std::size_t>::max() - a_right) {
        return false;
    }

    a_result = a_left + a_right;
    return true;
}

bool CheckedMul(const std::size_t a_left, const std::size_t a_right, std::size_t& a_result) {
    if (a_left != 0 && a_right > std::numeric_limits<std::size_t>::max() / a_left) {
        return false;
    }

    a_result = a_left * a_right;
    return true;
}

std::size_t MipExtent(const std::size_t a_extent, const std::size_t a_level) {
    return std::max<std::size_t>(a_extent >> a_level, 1);
}

struct DdsDimensions {
    std::size_t width;
    std::size_t height;
    std::size_t depth;
    std::size_t faces;
    std::size_t levels;
};

bool HasPayloadDimensions(const DdsDimensions& a_dimensions) {
    return a_dimensions.width != 0
           && a_dimensions.height != 0
           && a_dimensions.depth != 0
           && a_dimensions.faces != 0
           && a_dimensions.levels != 0
           && a_dimensions.levels <= kMaxGliTextureLevels;
}

std::size_t BlockCount(const std::size_t a_extent, const int a_blockExtent) {
    const auto blockExtent = static_cast<std::size_t>(a_blockExtent);
    return (a_extent + blockExtent - 1) / blockExtent;
}

std::optional<std::size_t> DdsPayloadSize(const gli::format a_format, const DdsDimensions& a_dimensions) {
    const auto blockSize = gli::block_size(a_format);
    const auto blockExtent = gli::block_extent(a_format);
    if (blockSize == 0
        || glm::any(glm::lessThanEqual(blockExtent, gli::extent3d(0)))
        || !HasPayloadDimensions(a_dimensions)) {
        return std::nullopt;
    }

    std::size_t payloadSize = 0;
    for (std::size_t level = 0; level < a_dimensions.levels; ++level) {
        const auto blocksX = BlockCount(MipExtent(a_dimensions.width, level), blockExtent.x);
        const auto blocksY = BlockCount(MipExtent(a_dimensions.height, level), blockExtent.y);
        const auto blocksZ = BlockCount(MipExtent(a_dimensions.depth, level), blockExtent.z);

        std::size_t levelSize = 0;
        if (!CheckedMul(blocksX, blocksY, levelSize)
            || !CheckedMul(levelSize, blocksZ, levelSize)
            || !CheckedMul(levelSize, blockSize, levelSize)
            || !CheckedAdd(payloadSize, levelSize, payloadSize)) {
            return std::nullopt;
        }
    }

    if (!CheckedMul(payloadSize, a_dimensions.faces, payloadSize)) {
        return std::nullopt;
    }
    return payloadSize;
}

bool HasLegacyPixelFormat(const DdsPixelFormat& a_format) {
    return (FlagBits(a_format.flags) & kLegacyPixelFormatFlags) != 0U && a_format.bpp != 0 && a_format.bpp < 64;
}

gli::format LegacyDdsFormat(const DdsHeader& a_header) {
    const gli::dx translator;

    if (!HasLegacyPixelFormat(a_header.Format)) {
        return gli::FORMAT_UNDEFINED;
    }

    for (const auto& candidate : kLegacyDdsFormatCandidates) {
        if (candidate.bitsPerPixel == a_header.Format.bpp
            && SameMask(a_header.Format.Mask, translator.translate(candidate.format).Mask)) {
            return candidate.format;
        }
    }

    return gli::FORMAT_UNDEFINED;
}

DdsHeader10 EmptyDdsHeader10() {
    DdsHeader10 header;
    std::memset(&header, 0, sizeof(header));
    return header;
}

DdsHeader EmptyDdsHeader() {
    DdsHeader header;
    std::memset(&header, 0, sizeof(header));
    return header;
}

class DdsTextureReader {
public:
    DdsTextureReader(const char* a_data, const std::size_t a_size)
        : _data(a_data, a_size)
        , _header(EmptyDdsHeader())
        , _header10(EmptyDdsHeader10()) {}

    gli::texture Load() {
        if (!ReadHeader()) {
            return {};
        }

        const auto format = ResolveFormat();
        if (format == gli::FORMAT_UNDEFINED) {
            return {};
        }

        const auto target = gli::detail::get_target(_header, _header10);
        if (!IsSupportedTarget(target)) {
            return {};
        }

        const auto mipMapCount = this->MipMapCount();
        if (mipMapCount == 0) {
            return {};
        }

        const auto faceCount = this->FaceCount();
        if (!HasValidFaceCount(target, faceCount)) {
            return {};
        }

        const auto layerCount = this->LayerCount();
        if (layerCount != 1) {
            return {};
        }

        const auto depthCount = this->DepthCount();
        if (depthCount == 0) {
            return {};
        }

        const auto textureExtent = gli::texture::extent_type(_header.Width, _header.Height, depthCount);
        if (!HasValidMipCount(textureExtent, mipMapCount)
            || !HasValidPayload(format, depthCount, faceCount, mipMapCount)) {
            return {};
        }

        gli::texture texture(target, format, textureExtent, layerCount, faceCount, mipMapCount);
        if (!CanCopyTexture(texture)) {
            return {};
        }

        std::memcpy(texture.data(), _data.subspan(_offset).data(), texture.size());
        return texture;
    }

private:
    bool ReadHeader() {
        if (!HasBaseHeader()) {
            return false;
        }

        _offset = sizeof(gli::detail::FOURCC_DDS);
        std::memcpy(&_header, _data.subspan(_offset).data(), sizeof(_header));
        _offset += sizeof(DdsHeader);

        if (!HasValidHeader()) {
            return false;
        }

        return !HasDx10Header() || ReadDx10Header();
    }

    [[nodiscard]] bool HasBaseHeader() const {
        return (_data.data() != nullptr)
               && _data.size() >= sizeof(gli::detail::FOURCC_DDS) + sizeof(DdsHeader)
               && std::strncmp(_data.data(), gli::detail::FOURCC_DDS, sizeof(gli::detail::FOURCC_DDS)) == 0;
    }

    [[nodiscard]] bool HasValidHeader() const {
        return _header.Size == sizeof(DdsHeader)
               && _header.Format.size == sizeof(DdsPixelFormat)
               && _header.Width != 0
               && _header.Height != 0;
    }

    [[nodiscard]] bool HasFourCc() const {
        return (FlagBits(_header.Format.flags) & FlagBits(gli::dx::DDPF_FOURCC)) != 0U;
    }

    [[nodiscard]] bool HasDx10Header() const {
        return HasFourCc()
               && (_header.Format.fourCC == gli::dx::D3DFMT_DX10 || _header.Format.fourCC == gli::dx::D3DFMT_GLI1);
    }

    bool ReadDx10Header() {
        if (_data.size() < _offset + sizeof(DdsHeader10)) {
            return false;
        }

        std::memcpy(&_header10, _data.subspan(_offset).data(), sizeof(_header10));
        _offset += sizeof(DdsHeader10);
        return true;
    }

    [[nodiscard]] gli::format ResolveFormat() const {
        const gli::dx dxTranslator;
        const auto format = LegacyDdsFormat(_header);

        if (HasDx10Header()) {
            return dxTranslator.find(_header.Format.fourCC, _header10.Format);
        }

        if (HasFourCc() && format == gli::FORMAT_UNDEFINED) {
            return dxTranslator.find(gli::detail::remap_four_cc(_header.Format.fourCC));
        }

        return format;
    }

    [[nodiscard]] static bool IsSupportedTarget(const gli::target a_target) {
        return a_target == gli::TARGET_2D || a_target == gli::TARGET_CUBE;
    }

    [[nodiscard]] std::size_t MipMapCount() const {
        return ((_header.Flags & FlagBits(gli::detail::DDSD_MIPMAPCOUNT)) != 0U) ? _header.MipMapLevels : 1;
    }

    [[nodiscard]] std::size_t FaceCount() const {
        if ((_header.CubemapFlags & FlagBits(gli::detail::DDSCAPS2_CUBEMAP)) != 0U) {
            return static_cast<std::size_t>(
                glm::bitCount(_header.CubemapFlags & FlagBits(gli::detail::DDSCAPS2_CUBEMAP_ALLFACES))
            );
        }

        if ((_header10.MiscFlag & FlagBits(gli::detail::D3D10_RESOURCE_MISC_TEXTURECUBE)) != 0U) {
            return 6;
        }

        return 1;
    }

    [[nodiscard]] static bool HasValidFaceCount(const gli::target a_target, const std::size_t a_faceCount) {
        return (a_target == gli::TARGET_2D && a_faceCount == 1) || (a_target == gli::TARGET_CUBE && a_faceCount == 6);
    }

    [[nodiscard]] std::size_t LayerCount() const {
        return std::max<gli::texture::size_type>(_header10.ArraySize, 1);
    }

    [[nodiscard]] std::size_t DepthCount() const {
        return ((_header.CubemapFlags & FlagBits(gli::detail::DDSCAPS2_VOLUME)) != 0U) ? _header.Depth : 1;
    }

    [[nodiscard]] static bool HasValidMipCount(
        const gli::texture::extent_type a_textureExtent,
        const std::size_t a_mipMapCount
    ) {
        return a_mipMapCount <= std::min<std::size_t>(gli::levels(a_textureExtent), kMaxGliTextureLevels);
    }

    [[nodiscard]] bool HasValidPayload(
        const gli::format a_format,
        const std::size_t a_depthCount,
        const std::size_t a_faceCount,
        const std::size_t a_mipMapCount
    ) const {
        const auto payloadSize = DdsPayloadSize(
            a_format,
            {
                .width = _header.Width,
                .height = _header.Height,
                .depth = a_depthCount,
                .faces = a_faceCount,
                .levels = a_mipMapCount,
            }
        );
        return payloadSize.has_value() && *payloadSize <= _data.size() - _offset;
    }

    [[nodiscard]] bool CanCopyTexture(const gli::texture& a_texture) const {
        return !a_texture.empty() && a_texture.layers() == 1 && a_texture.size() <= _data.size() - _offset;
    }

    std::span<const char> _data;
    std::size_t _offset = 0;
    DdsHeader _header;
    DdsHeader10 _header10;
};

} // namespace

gli::texture DdsTextures::LoadIfValid(const char* a_data, const std::size_t a_size) {
    DdsTextureReader reader(a_data, a_size);
    return reader.Load();
}

gli::texture DdsTextures::LoadFileIfValid(const QString& a_path) {
    QFile file(a_path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    const auto data = file.readAll();
    return LoadIfValid(data.constData(), static_cast<std::size_t>(data.size()));
}
