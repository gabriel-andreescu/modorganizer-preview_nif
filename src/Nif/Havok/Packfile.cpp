#include "Nif/Havok/Packfile.h"

#include <QString>

#include <algorithm>
#include <array>
#include <cstddef>
#include <iterator>
#include <optional>
#include <utility>

namespace Havok {
namespace {
    constexpr std::array<std::uint32_t, 2> kMagic {0x57E0E057U, 0x10C0C010U};
    constexpr std::size_t kHeaderSize = 64;
    constexpr std::size_t kSectionHeaderSize = 64;
    constexpr std::size_t kSectionTagSize = 20;
    constexpr std::int32_t kEndOfFixups = -1;
    // The class layouts this module reads are those of this release, so other versions would decode as garbage.
    constexpr std::string_view kVersion = "hk_2014.1.0-r1";

    struct Section {
        std::string tag;
        std::uint32_t start = 0;
        std::uint32_t localFixups = 0;
        std::uint32_t globalFixups = 0;
        std::uint32_t virtualFixups = 0;
        std::uint32_t exports = 0;
    };

    class FileReader {
    public:
        explicit FileReader(const std::span<const char> a_bytes)
            : _bytes(a_bytes) {}

        template <class T>
        [[nodiscard]] T Read(const std::uint64_t a_offset) const {
            CheckRange(a_offset, sizeof(T));
            T value;
            std::memcpy(&value, _bytes.subspan(a_offset).data(), sizeof(T));
            return value;
        }

        [[nodiscard]] std::string String(const std::uint64_t a_offset, const std::uint64_t a_end) const {
            CheckRange(a_offset, 0);
            const auto limit = std::min<std::uint64_t>(a_end, _bytes.size());
            const auto bytes = _bytes.subspan(a_offset, limit > a_offset ? limit - a_offset : 0);
            const auto end = std::ranges::find(bytes, '\0');
            if (end == bytes.end()) {
                throw std::runtime_error(
                    QStringLiteral("Havok packfile string at offset %1 has no terminator").arg(a_offset).toStdString()
                );
            }
            return {bytes.begin(), end};
        }

        [[nodiscard]] std::span<const char> Bytes(const std::uint64_t a_offset, const std::uint64_t a_size) const {
            CheckRange(a_offset, a_size);
            return _bytes.subspan(a_offset, a_size);
        }

        void CheckRange(const std::uint64_t a_offset, const std::uint64_t a_size) const {
            if (a_offset > _bytes.size() || a_size > _bytes.size() - a_offset) {
                throw std::runtime_error(
                    QStringLiteral("Havok packfile is truncated: %1 bytes at offset %2 exceed its %3 bytes")
                        .arg(a_size)
                        .arg(a_offset)
                        .arg(_bytes.size())
                        .toStdString()
                );
            }
        }

    private:
        std::span<const char> _bytes;
    };

    void CheckHeader(const FileReader& a_file) {
        if (a_file.Read<std::uint32_t>(0) != kMagic[0] || a_file.Read<std::uint32_t>(4) != kMagic[1]) {
            throw std::runtime_error("Havok packfile has no packfile signature");
        }

        const auto pointerSize = a_file.Read<std::uint8_t>(16);
        const auto littleEndian = a_file.Read<std::uint8_t>(17);
        if (pointerSize != 8 || littleEndian != 1) {
            throw std::runtime_error(QStringLiteral(
                "Havok packfile layout has %1-byte pointers and endian flag %2, expected 64-bit little-endian"
            )
                    .arg(pointerSize)
                    .arg(littleEndian)
                    .toStdString());
        }

        const auto version = a_file.String(40, 56);
        if (version != kVersion) {
            throw std::runtime_error(QStringLiteral("Havok packfile version '%1' is not %2")
                    .arg(QString::fromStdString(version))
                    .arg(QString::fromUtf8(kVersion.data(), static_cast<qsizetype>(kVersion.size())))
                    .toStdString());
        }
    }

    std::vector<Section> ReadSections(const FileReader& a_file) {
        const auto count = a_file.Read<std::int32_t>(20);
        const auto predicateSize = a_file.Read<std::int16_t>(62);
        const auto first = kHeaderSize + static_cast<std::size_t>(std::max<std::int16_t>(predicateSize, 0));
        if (count < 0) {
            throw std::runtime_error(QStringLiteral("Havok packfile has %1 sections").arg(count).toStdString());
        }

        std::vector<Section> sections;
        for (std::size_t i = 0; std::cmp_less(i, count); i++) {
            const auto header = first + (i * kSectionHeaderSize);
            sections.push_back({
                .tag = a_file.String(header, header + kSectionTagSize),
                .start = a_file.Read<std::uint32_t>(header + kSectionTagSize),
                .localFixups = a_file.Read<std::uint32_t>(header + kSectionTagSize + 4),
                .globalFixups = a_file.Read<std::uint32_t>(header + kSectionTagSize + 8),
                .virtualFixups = a_file.Read<std::uint32_t>(header + kSectionTagSize + 12),
                .exports = a_file.Read<std::uint32_t>(header + kSectionTagSize + 16),
            });
        }
        return sections;
    }

    std::size_t FindSection(const std::vector<Section>& a_sections, const std::string_view a_tag) {
        const auto section = std::ranges::find(a_sections, a_tag, &Section::tag);
        if (section == a_sections.end()) {
            throw std::runtime_error(QStringLiteral("Havok packfile has no %1 section")
                    .arg(QString::fromUtf8(a_tag.data(), static_cast<qsizetype>(a_tag.size())))
                    .toStdString());
        }
        return static_cast<std::size_t>(std::distance(a_sections.begin(), section));
    }
} // namespace

Packfile::Packfile(const std::span<const char> a_bytes) {
    const FileReader file(a_bytes);
    CheckHeader(file);

    const auto sections = ReadSections(file);
    const auto dataIndex = FindSection(sections, "__data__");
    const auto& classNames = sections[FindSection(sections, "__classnames__")];
    const auto& data = sections[dataIndex];
    if (data.localFixups > data.globalFixups
        || data.globalFixups > data.virtualFixups
        || data.virtualFixups > data.exports) {
        throw std::runtime_error("Havok packfile data section has fixup tables out of order");
    }

    const auto bytes = file.Bytes(data.start, data.localFixups);
    _data.assign(bytes.begin(), bytes.end());

    for (std::uint64_t entry = data.start + data.localFixups; entry + 8 <= data.start + data.globalFixups; entry += 8) {
        const auto source = file.Read<std::int32_t>(entry);
        if (source == kEndOfFixups) {
            break;
        }
        _pointers[static_cast<std::uint32_t>(source)] = file.Read<std::uint32_t>(entry + 4);
    }

    for (
        std::uint64_t entry = data.start + data.globalFixups; entry + 12 <= data.start + data.virtualFixups; entry += 12
    ) {
        const auto source = file.Read<std::int32_t>(entry);
        if (source == kEndOfFixups) {
            break;
        }
        if (file.Read<std::uint32_t>(entry + 4) == dataIndex) {
            _pointers[static_cast<std::uint32_t>(source)] = file.Read<std::uint32_t>(entry + 8);
        }
    }

    const auto classNamesEnd = std::uint64_t {classNames.start} + classNames.localFixups;
    for (std::uint64_t entry = data.start + data.virtualFixups; entry + 12 <= data.start + data.exports; entry += 12) {
        const auto source = file.Read<std::int32_t>(entry);
        if (source == kEndOfFixups) {
            break;
        }
        const auto name = std::uint64_t {classNames.start} + file.Read<std::uint32_t>(entry + 8);
        _objects[static_cast<std::uint32_t>(source)] = file.String(name, classNamesEnd);
    }
}

std::uint32_t Packfile::FindObject(const std::string_view a_className) const {
    std::optional<std::uint32_t> found;
    for (const auto& [offset, className] : _objects) {
        if (className == a_className && (!found || offset < *found)) {
            found = offset;
        }
    }
    if (!found) {
        throw std::runtime_error(QStringLiteral("Havok packfile has no %1 object")
                .arg(QString::fromUtf8(a_className.data(), static_cast<qsizetype>(a_className.size())))
                .toStdString());
    }
    return *found;
}

std::string_view Packfile::ClassOf(const std::uint32_t a_offset) const {
    const auto object = _objects.find(a_offset);
    if (object == _objects.end()) {
        throw std::runtime_error(
            QStringLiteral("Havok packfile has no object at data offset %1").arg(a_offset).toStdString()
        );
    }
    return object->second;
}

bool Packfile::HasPointer(const std::uint32_t a_offset) const {
    return _pointers.contains(a_offset);
}

std::uint32_t Packfile::Pointer(const std::uint32_t a_offset) const {
    const auto pointer = _pointers.find(a_offset);
    if (pointer == _pointers.end()) {
        throw std::runtime_error(
            QStringLiteral("Havok packfile has no pointer at data offset %1").arg(a_offset).toStdString()
        );
    }
    return pointer->second;
}

Packfile::Range Packfile::Array(const std::uint32_t a_offset) const {
    const auto count = Read<std::int32_t>(std::uint64_t {a_offset} + 8);
    if (count < 0) {
        throw std::runtime_error(
            QStringLiteral("Havok array at data offset %1 has %2 elements").arg(a_offset).arg(count).toStdString()
        );
    }
    if (count == 0) {
        return {};
    }
    return {.offset = Pointer(a_offset), .count = static_cast<std::uint32_t>(count)};
}

Packfile::Range Packfile::RelArray(const std::uint32_t a_offset) const {
    // A relative array stores its size, then its start relative to its own address.
    const auto count = Read<std::uint16_t>(a_offset);
    const auto start = Read<std::uint16_t>(std::uint64_t {a_offset} + 2);
    return {.offset = a_offset + start, .count = count};
}

glm::vec3 Packfile::Vector(const std::uint32_t a_offset) const {
    return {
        Read<float>(a_offset),
        Read<float>(std::uint64_t {a_offset} + 4),
        Read<float>(std::uint64_t {a_offset} + 8),
    };
}

void Packfile::CheckRange(const std::uint64_t a_offset, const std::uint64_t a_size) const {
    if (a_offset > _data.size() || a_size > _data.size() - a_offset) {
        throw std::runtime_error(
            QStringLiteral("Havok data read of %1 bytes at offset %2 exceeds the %3-byte data section")
                .arg(a_size)
                .arg(a_offset)
                .arg(_data.size())
                .toStdString()
        );
    }
}
} // namespace Havok
