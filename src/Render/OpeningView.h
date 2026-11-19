#pragma once

#include <nifly/Object3d.hpp>

struct ViewAngles {
    float yaw = 0.0F;
    float pitch = 0.0F;
};

// The angles a mesh opens at, from the size of its bounding box in NIF space. The view faces the mesh's front unless
// its side is clearly larger, and looks down on a mesh that is flat, so a blade shows its flat and a rug its top.
// A skinned mesh is part of an actor, which faces the front, so it turns to its side only when it is far longer than
// wide, as a horse is and a head with hair is not.
[[nodiscard]] ViewAngles OpeningView(const nifly::Vector3& a_size, bool a_skinned);
