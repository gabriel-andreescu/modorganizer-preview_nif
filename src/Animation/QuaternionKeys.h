#pragma once

#include <nifly/Animation.hpp>

#include <vector>

namespace Animation {

// A transform's quaternion keys, made ready to sample the way the games ready them when they load the file.
class QuaternionKeys {
public:
    // The data must have at least one quaternion key.
    explicit QuaternionKeys(const nifly::NiKeyframeData& a_data);

    // Before the first key and after the last it is that key's rotation.
    [[nodiscard]] nifly::Quaternion At(float a_time) const;

private:
    struct Key {
        float time = 0.0F;
        nifly::Quaternion value;
        // The control rotations a smooth key's curve bends toward, on the way out of the key and on the way into it.
        nifly::Quaternion leaving;
        nifly::Quaternion arriving;
    };

    void TurnToSameSide();
    void BendSmoothKeys(const std::vector<nifly::NiAnimationKey<nifly::Quaternion>>& a_source);

    std::vector<Key> _keys;
    nifly::NiKeyType _type = nifly::LINEAR_KEY;
};

} // namespace Animation
