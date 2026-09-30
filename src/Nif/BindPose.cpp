#include "Nif/BindPose.h"
#include "Nif/Transforms.h"

#include <Eigen/Dense>

#include <nifly/NifFile.hpp>

#include <cstdint>
#include <vector>

namespace {
// Bones in a line or a single bone leave the rotation about that line free.
constexpr double kDeterminedSpread = 1e-3;
constexpr unsigned kFullSvd = static_cast<unsigned>(Eigen::ComputeFullU) | static_cast<unsigned>(Eigen::ComputeFullV);

struct BonePositions {
    Eigen::Vector3d skin;
    Eigen::Vector3d model;
};

Eigen::Vector3d ToEigen(const nifly::Vector3& a_value) {
    return {a_value.x, a_value.y, a_value.z};
}

nifly::Vector3 ToNifly(const Eigen::Vector3d& a_value) {
    return {static_cast<float>(a_value.x()), static_cast<float>(a_value.y()), static_cast<float>(a_value.z())};
}

std::vector<BonePositions> CollectBonePositions(nifly::NifFile* a_nifFile, nifly::NiShape* a_shape) {
    std::vector<int> boneIds;
    a_nifFile->GetShapeBoneIDList(a_shape, boneIds);

    std::vector<BonePositions> positions;
    for (std::uint32_t index = 0; index < boneIds.size(); ++index) {
        auto* const bone = a_nifFile->GetHeader().GetBlock<nifly::NiNode>(boneIds[index]);
        nifly::MatTransform skinToBone;
        if (!bone || !a_nifFile->GetShapeTransformSkinToBone(a_shape, index, skinToBone)) {
            continue;
        }

        positions.push_back({
            .skin = ToEigen(skinToBone.InverseTransform().translation),
            .model = ToEigen(GetObjectTransformToGlobal(a_nifFile, bone).translation),
        });
    }
    return positions;
}

// Kabsch: the rotation that best maps the centered skin positions onto the centered model positions.
Eigen::Matrix3d FitRotation(const std::vector<BonePositions>& a_positions, const BonePositions& a_centroid) {
    Eigen::Matrix3d covariance = Eigen::Matrix3d::Zero();
    for (const auto& [skin, model] : a_positions) {
        covariance += (skin - a_centroid.skin) * (model - a_centroid.model).transpose();
    }

    const Eigen::JacobiSVD<Eigen::Matrix3d> svd(covariance, kFullSvd);
    const auto& spread = svd.singularValues();
    // Vanilla skin spaces share the model's axes, apart from FaceGen meatcaps, so an undetermined rotation keeps them.
    if (spread(0) <= 0.0 || spread(1) <= kDeterminedSpread * spread(0)) {
        return Eigen::Matrix3d::Identity();
    }

    Eigen::Matrix3d reflection = Eigen::Matrix3d::Identity();
    reflection(2, 2) = (svd.matrixV() * svd.matrixU().transpose()).determinant() < 0.0 ? -1.0 : 1.0;
    return svd.matrixV() * reflection * svd.matrixU().transpose();
}
} // namespace

std::optional<nifly::MatTransform> FitSkinToModel(nifly::NifFile* a_nifFile, nifly::NiShape* a_shape) {
    const auto positions = CollectBonePositions(a_nifFile, a_shape);
    if (positions.empty()) {
        return std::nullopt;
    }

    BonePositions centroid {.skin = Eigen::Vector3d::Zero(), .model = Eigen::Vector3d::Zero()};
    for (const auto& [skin, model] : positions) {
        centroid.skin += skin;
        centroid.model += model;
    }
    centroid.skin /= static_cast<double>(positions.size());
    centroid.model /= static_cast<double>(positions.size());

    const auto rotation = FitRotation(positions, centroid);
    nifly::MatTransform skinToModel;
    for (int row = 0; row < 3; ++row) {
        skinToModel.rotation[row] = ToNifly(rotation.row(row).transpose());
    }
    skinToModel.translation = ToNifly(centroid.model - rotation * centroid.skin);
    return skinToModel;
}
