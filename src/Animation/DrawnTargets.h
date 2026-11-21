#pragma once

#include <cstdint>
#include <vector>

namespace nifly {
class NifFile;
class NiAVObject;
}

namespace Animation {

// What animation can show in the preview: the shapes that can be drawn, with their shaders, their bones and the
// nodes above them. Animation on anything else, such as a particle system's shader or a camera node, shows nothing.
class DrawnTargets {
public:
    explicit DrawnTargets(const nifly::NifFile& a_nifFile);

    [[nodiscard]] bool HasShader(std::uint32_t a_block) const;
    [[nodiscard]] bool HasObject(std::uint32_t a_block) const;

private:
    void AddWithParents(const nifly::NifFile& a_nifFile, nifly::NiAVObject* a_object);

    // Sorted, to search.
    std::vector<std::uint32_t> _shaders;
    std::vector<std::uint32_t> _objects;
};

// Whether a NIF has a controller that shows and hides objects, so a shape that is hidden at rest can appear.
[[nodiscard]] bool AnimatesVisibility(const nifly::NifFile& a_nifFile);

} // namespace Animation
