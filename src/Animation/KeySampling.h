#pragma once

#include <nifly/Keys.hpp>

namespace Animation {

namespace Detail {
    // The curve from one value to the next over a_x in 0..1, leaving with a_out and arriving with a_in.
    template <typename T>
    T Hermite(const T& a_from, const T& a_to, const T& a_out, const T& a_in, const float a_x) {
        const auto squared = a_x * a_x;
        const auto cubed = squared * a_x;
        return (a_from * ((2.0F * cubed) - (3.0F * squared) + 1.0F))
               + (a_to * ((-2.0F * cubed) + (3.0F * squared)))
               + (a_out * (cubed - (2.0F * squared) + a_x))
               + (a_in * (cubed - squared));
    }

    // A tension, bias and continuity key's tangent, leaving the key or arriving at it. The keys either side shape it,
    // and an end key uses the one neighbor it has.
    template <typename T>
    T TbcTangent(const nifly::NiAnimationKeyGroup<T>& a_keys, const int a_index, const bool a_leaving) {
        const auto last = static_cast<int>(a_keys.GetNumKeys()) - 1;
        const auto key = a_keys.GetKey(a_index);
        const auto previous = a_keys.GetKey(a_index > 0 ? a_index - 1 : a_index);
        const auto next = a_keys.GetKey(a_index < last ? a_index + 1 : a_index);
        const auto fromPrevious = a_index > 0 ? key.value - previous.value : next.value - key.value;
        const auto toNext = a_index < last ? next.value - key.value : key.value - previous.value;

        // The games flip the bias for the arriving tangent. The textbook curve flips the continuity there, which
        // bends keys with either value differently from the game.
        const auto tension = 0.5F * (1.0F - key.tbc.tension);
        const auto bias = a_leaving ? key.tbc.bias : -key.tbc.bias;
        const auto tangent = (fromPrevious * (tension * (1.0F + bias) * (1.0F + key.tbc.continuity)))
                             + (toNext * (tension * (1.0F - bias) * (1.0F - key.tbc.continuity)));
        if (a_index == 0 || a_index == last) {
            return tangent;
        }

        // Keys that are unevenly spaced in time scale the tangent by its side's share of the span.
        const auto side = a_leaving ? next.time - key.time : key.time - previous.time;
        return tangent * (2.0F * side / (next.time - previous.time));
    }
} // namespace Detail

// The value of a group with at least one key at a time. Before the first key and after the last it is that key's
// value.
template <typename T>
T SampleKeys(const nifly::NiAnimationKeyGroup<T>& a_keys, const float a_time) {
    const auto count = static_cast<int>(a_keys.GetNumKeys());
    if (const auto first = a_keys.GetKey(0); count == 1 || a_time <= first.time) {
        return first.value;
    }
    if (const auto last = a_keys.GetKey(count - 1); a_time >= last.time) {
        return last.value;
    }

    auto low = 0;
    auto high = count - 1;
    while (high - low > 1) {
        const auto middle = (low + high) / 2;
        (a_keys.GetKey(middle).time <= a_time ? low : high) = middle;
    }

    const auto before = a_keys.GetKey(low);
    const auto after = a_keys.GetKey(high);
    const auto along = (a_time - before.time) / (after.time - before.time);
    switch (a_keys.GetInterpolationType()) {
        case nifly::QUADRATIC_KEY:
            return Detail::Hermite(before.value, after.value, before.backward, after.forward, along);
        case nifly::TBC_KEY:
            return Detail::Hermite(
                before.value,
                after.value,
                Detail::TbcTangent(a_keys, low, true),
                Detail::TbcTangent(a_keys, high, false),
                along
            );
        case nifly::CONST_KEY: return before.value;
        default:               return before.value + ((after.value - before.value) * along);
    }
}

} // namespace Animation
