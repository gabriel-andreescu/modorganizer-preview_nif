#include "Nif/Havok/PhysicsSystem.h"

#include "Nif/Havok/Shapes.h"

#include <QString>

#include <glm/mat4x4.hpp>

#include <stdexcept>

namespace Havok {
namespace {
    constexpr std::uint32_t kBodyCinfos = 0x40;
    constexpr std::uint32_t kBodyCinfoSize = 0x60;
    constexpr std::uint32_t kBodyShape = 0x00;
    constexpr std::uint32_t kBodyCollisionFilterInfo = 0x14;
} // namespace

PhysicsSystem::PhysicsSystem(const std::span<const char> a_bytes)
    : _packfile(a_bytes)
    , _bodies(_packfile.Array(_packfile.FindObject("hknpPhysicsSystemData") + kBodyCinfos)) {}

// Body infos also hold the body's model-space position, but the game places each body at its collision object's
// node, and the shapes are authored in that node's space.
Body PhysicsSystem::ReadBody(const std::uint32_t a_bodyId) const {
    if (a_bodyId >= _bodies.count) {
        throw std::runtime_error(QStringLiteral("Havok physics system has %1 bodies, so body %2 does not exist")
                .arg(_bodies.count)
                .arg(a_bodyId)
                .toStdString());
    }

    const auto body = _bodies.offset + (a_bodyId * kBodyCinfoSize);
    Body result {.collisionFilterInfo = _packfile.Read<std::uint32_t>(body + kBodyCollisionFilterInfo)};
    ReadShape(_packfile, _packfile.Pointer(body + kBodyShape), glm::mat4(1.0F), result.geometry);
    return result;
}
} // namespace Havok
