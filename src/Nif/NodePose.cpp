#include "Nif/NodePose.h"

#include <algorithm>

void NodePose::SetLocal(const std::uint32_t a_block, const nifly::MatTransform& a_transform) {
    const auto found = std::ranges::find(_locals, a_block, &std::pair<std::uint32_t, nifly::MatTransform>::first);
    if (found != _locals.end()) {
        found->second = a_transform;
    } else {
        _locals.emplace_back(a_block, a_transform);
    }
}

void NodePose::SetHidden(const std::uint32_t a_block, const bool a_hidden) {
    const auto found = std::ranges::find(_hidden, a_block, &std::pair<std::uint32_t, bool>::first);
    if (found != _hidden.end()) {
        found->second = a_hidden;
    } else {
        _hidden.emplace_back(a_block, a_hidden);
    }
}

const nifly::MatTransform* NodePose::Local(const std::uint32_t a_block) const {
    const auto found = std::ranges::find(_locals, a_block, &std::pair<std::uint32_t, nifly::MatTransform>::first);
    return found != _locals.end() ? &found->second : nullptr;
}

std::optional<bool> NodePose::Hidden(const std::uint32_t a_block) const {
    const auto found = std::ranges::find(_hidden, a_block, &std::pair<std::uint32_t, bool>::first);
    return found != _hidden.end() ? std::optional(found->second) : std::nullopt;
}
