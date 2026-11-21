#pragma once

#include <nifly/Object3d.hpp>

#include <algorithm>

// An axis-aligned box in NIF space. It holds nothing until a point is added.
struct BoundingBox {
    nifly::Vector3 min;
    nifly::Vector3 max;
    bool empty = true;

    void Add(const nifly::Vector3& a_point) {
        if (empty) {
            min = a_point;
            max = a_point;
            empty = false;
            return;
        }
        min = {std::min(min.x, a_point.x), std::min(min.y, a_point.y), std::min(min.z, a_point.z)};
        max = {std::max(max.x, a_point.x), std::max(max.y, a_point.y), std::max(max.z, a_point.z)};
    }

    void Add(const BoundingBox& a_box) {
        if (!a_box.empty) {
            Add(a_box.min);
            Add(a_box.max);
        }
    }

    // Zero for a box that holds nothing.
    [[nodiscard]] nifly::Vector3 Size() const {
        return empty ? nifly::Vector3() : max - min;
    }
};
