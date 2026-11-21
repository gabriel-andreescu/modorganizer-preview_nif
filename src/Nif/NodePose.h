#pragma once

#include <nifly/Object3d.hpp>

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

// The local transforms and hidden states an animation gives objects at one moment, in place of the ones the NIF
// stores for them. Objects are named by their block.
class NodePose {
public:
    void SetLocal(std::uint32_t a_block, const nifly::MatTransform& a_transform);
    void SetHidden(std::uint32_t a_block, bool a_hidden);

    [[nodiscard]] const nifly::MatTransform* Local(std::uint32_t a_block) const;
    [[nodiscard]] std::optional<bool> Hidden(std::uint32_t a_block) const;
    [[nodiscard]] bool Empty() const noexcept {
        return _locals.empty() && _hidden.empty();
    }

private:
    // A mesh animates a handful of objects, so they are searched in order.
    std::vector<std::pair<std::uint32_t, nifly::MatTransform>> _locals;
    std::vector<std::pair<std::uint32_t, bool>> _hidden;
};
