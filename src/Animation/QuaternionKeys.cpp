#include "Animation/QuaternionKeys.h"
#include "Animation/Quaternions.h"

#include <algorithm>
#include <cstddef>

namespace Animation {

QuaternionKeys::QuaternionKeys(const nifly::NiKeyframeData& a_data)
    : _type {a_data.rotationType} {
    _keys.reserve(a_data.quaternionKeys.size());
    for (const auto& key : a_data.quaternionKeys) {
        _keys.push_back({.time = key.time, .value = key.value, .leaving = key.value, .arriving = key.value});
    }

    TurnToSameSide();
    if (_type == nifly::TBC_KEY && _keys.size() > 1) {
        BendSmoothKeys(a_data.quaternionKeys);
    }
}

nifly::Quaternion QuaternionKeys::At(const float a_time) const {
    if (_keys.size() == 1 || a_time <= _keys.front().time) {
        return _keys.front().value;
    }
    if (a_time >= _keys.back().time) {
        return _keys.back().value;
    }

    const auto after = std::ranges::upper_bound(_keys, a_time, {}, &Key::time);
    const auto before = std::prev(after);
    const auto along = (a_time - before->time) / (after->time - before->time);
    switch (_type) {
        case nifly::TBC_KEY:   return Squad(before->value, before->leaving, after->arriving, after->value, along);
        case nifly::CONST_KEY: return before->value;
        default:               return Slerp(before->value, after->value, along);
    }
}

// A quaternion and its negative are the same rotation, and the blend between two keys takes the long way round when
// they sit on opposite sides. Each key is flipped to the side of the one before it.
void QuaternionKeys::TurnToSameSide() {
    for (std::size_t i = 1; i < _keys.size(); ++i) {
        if (Dot(_keys[i - 1].value, _keys[i].value) < 0.0F) {
            _keys[i].value = Negated(_keys[i].value);
        }
    }
    for (auto& key : _keys) {
        key.value.w = std::clamp(key.value.w, -1.0F, 1.0F);
    }
}

// An end key stands in for the neighbor it lacks, which leaves that side's turn at zero.
void QuaternionKeys::BendSmoothKeys(const std::vector<nifly::NiAnimationKey<nifly::Quaternion>>& a_source) {
    const auto last = _keys.size() - 1;
    for (std::size_t i = 0; i <= last; ++i) {
        auto& key = _keys[i];
        const auto& previous = _keys[i > 0 ? i - 1 : i];
        const auto& next = _keys[i < last ? i + 1 : i];
        const auto& shape = a_source[i].tbc;

        const auto fromPrevious = Log(Product(Inverse(previous.value), key.value));
        const auto toNext = Log(Product(Inverse(key.value), next.value));
        const auto tightness = (1.0F - shape.tension) / (next.time - previous.time);

        const auto leavingShare = (key.time - previous.time) * tightness;
        const auto leavingTurn = (fromPrevious * (leavingShare * (1.0F + shape.bias) * (1.0F + shape.continuity)))
                                 + (toNext * (leavingShare * (1.0F - shape.bias) * (1.0F - shape.continuity)));
        key.leaving = Product(key.value, Exp((leavingTurn - toNext) * 0.5F));

        const auto arrivingShare = (next.time - key.time) * tightness;
        const auto arrivingTurn = (fromPrevious * (arrivingShare * (1.0F - shape.bias) * (1.0F + shape.continuity)))
                                  + (toNext * (arrivingShare * (1.0F + shape.bias) * (1.0F - shape.continuity)));
        key.arriving = Product(key.value, Exp((fromPrevious - arrivingTurn) * 0.5F));
    }
}

} // namespace Animation
