#include "Nif/ShapeVisibility.h"
#include "Nif/ShaderFlags.h"

#include <QString>

#include <nifly/NifFile.hpp>

#include <algorithm>
#include <iterator>
#include <utility>

namespace {
// The games remove every object whose name contains this, in any case, along with its subtree.
bool IsEditorMarker(const nifly::NiAVObject& a_object) {
    return QString::fromStdString(a_object.name.get()).contains(QStringLiteral("EditorMarker"), Qt::CaseInsensitive);
}

bool IsSelectedChild(const nifly::NifFile& a_nifFile, const nifly::NiSwitchNode& a_switch, nifly::NiObject* a_child) {
    const auto childId = a_nifFile.GetBlockID(a_child);
    const auto first = a_switch.childRefs.cbegin();
    const auto child = std::find_if(first, a_switch.childRefs.cend(), [&](const auto& a_ref) {
        return a_ref.index == childId;
    });
    return std::cmp_equal(std::distance(first, child), a_switch.index);
}
} // namespace

ShapeVisibility ClassifyShapeVisibility(const nifly::NifFile& a_nifFile, nifly::NiShape* a_shape) {
    bool editorMarker = false;
    nifly::NiAVObject* object = a_shape;
    while (object) {
        if ((object->flags & TriShape::kHidden) != 0U) {
            return ShapeVisibility::kHidden;
        }
        editorMarker = editorMarker || IsEditorMarker(*object);

        auto* const parent = a_nifFile.GetParentNode(object);
        if (const auto* const switchNode = dynamic_cast<const nifly::NiSwitchNode*>(parent);
            switchNode && !IsSelectedChild(a_nifFile, *switchNode, object)) {
            return ShapeVisibility::kInactiveSwitchChild;
        }
        object = parent;
    }
    return editorMarker ? ShapeVisibility::kEditorMarker : ShapeVisibility::kVisible;
}
