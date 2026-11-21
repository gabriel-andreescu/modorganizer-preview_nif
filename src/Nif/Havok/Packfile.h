#pragma once

#include <glm/vec3.hpp>

#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Havok {
// A Havok 2014 packfile with 64-bit little-endian layout, the format Fallout 4 embeds in bhkPhysicsSystem blocks.
// Offsets address the data section, whose pointers are resolved through the packfile's fixup tables.
class Packfile {
public:
    struct Range {
        std::uint32_t offset = 0;
        std::uint32_t count = 0;
    };

    explicit Packfile(std::span<const char> a_bytes);

    // The data offset of the packfile's top-level object.
    [[nodiscard]] std::uint32_t Root() const;
    [[nodiscard]] std::string_view ClassOf(std::uint32_t a_offset) const;
    [[nodiscard]] bool HasPointer(std::uint32_t a_offset) const;
    [[nodiscard]] std::uint32_t Pointer(std::uint32_t a_offset) const;
    [[nodiscard]] Range Array(std::uint32_t a_offset) const;
    [[nodiscard]] Range RelArray(std::uint32_t a_offset) const;
    [[nodiscard]] glm::vec3 Vector(std::uint32_t a_offset) const;

    template <class T>
    [[nodiscard]] T Read(const std::uint64_t a_offset) const {
        CheckRange(a_offset, sizeof(T));
        T value;
        std::memcpy(&value, std::span(_data).subspan(a_offset).data(), sizeof(T));
        return value;
    }

    void CheckRange(std::uint64_t a_offset, std::uint64_t a_size) const;

private:
    std::vector<char> _data;
    std::unordered_map<std::uint32_t, std::uint32_t> _pointers;
    std::unordered_map<std::uint32_t, std::string> _objects;
    std::uint32_t _root = 0;
};
} // namespace Havok
