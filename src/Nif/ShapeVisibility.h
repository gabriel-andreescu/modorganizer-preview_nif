#pragma once

namespace nifly {
class NifFile;
class NiShape;
}

enum class ShapeVisibility {
    kVisible,
    kHidden,
    kInactiveSwitchChild,
    kEditorMarker,
};

// Whether the game draws a shape, judged from the shape and every ancestor: the hidden flag, the child each switch
// node selects, and editor marker names.
[[nodiscard]] ShapeVisibility ClassifyShapeVisibility(const nifly::NifFile& a_nifFile, nifly::NiShape* a_shape);
