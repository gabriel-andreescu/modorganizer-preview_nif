#include "Render/ShapeRenderGeometry.h"
#include "Nif/BindPose.h"
#include "Nif/Transforms.h"
#include "Render/MeshProblems.h"

#include <nifly/NifFile.hpp>
#include <nifly/Skin.hpp>

#include <string>
#include <utility>

namespace {
const std::vector<nifly::Vector3>* VectorOrNull(const std::vector<nifly::Vector3>& a_values) {
    return a_values.empty() ? nullptr : &a_values;
}

std::string ShapeName(nifly::NiShape* a_shape) {
    const auto name = a_shape->name.get();
    return name.empty() ? std::string("<unnamed>") : name;
}

std::vector<nifly::Triangle> GetShapeTriangles(const nifly::NiShape* a_shape, nifly::NiSkinPartition* a_skinPartition) {
    std::vector<nifly::Triangle> triangles;

    if (a_skinPartition) {
        a_skinPartition->PrepareTrueTriangles();
        for (const auto& partition : a_skinPartition->partitions) {
            triangles.insert(triangles.end(), partition.trueTriangles.begin(), partition.trueTriangles.end());
        }
        if (!triangles.empty()) {
            return triangles;
        }
    }

    a_shape->GetTriangles(triangles);
    return triangles;
}

void UseSkinnedVertices(ShapeRenderGeometry& a_geometry, SkinnedVertices a_vertices) {
    a_geometry.deformed = std::move(a_vertices);
    a_geometry.skinned = true;
    a_geometry.bounds = nifly::BoundingSphere(a_geometry.deformed.positions);
    a_geometry.modelTransform.Clear();
}

bool TryBuildSkinnedGeometry(nifly::NifFile* a_nifFile, nifly::NiShape* a_shape, ShapeRenderGeometry& a_geometry) {
    if (!a_shape->HasSkinInstance() || !a_geometry.raw.positions || a_geometry.raw.positions->empty()) {
        return false;
    }

    const auto& header = a_nifFile->GetHeader();
    auto* const skinContainer = header.GetBlock<nifly::NiBoneContainer>(a_shape->SkinInstanceRef());
    if (!skinContainer) {
        ReportMeshProblem(QStringLiteral("Skipping skinning for NIF shape '%1': missing skin instance")
                .arg(QString::fromStdString(ShapeName(a_shape))));
        return false;
    }

    // Fallout 4 skin data stores no skin-to-model transform, and FaceGen bone nodes carry positions without
    // rotations, so skinning with them tears FaceGen shapes. The fitted bind pose places the shape as one piece.
    if (dynamic_cast<nifly::NiSkinInstance*>(skinContainer) == nullptr) {
        if (const auto skinToModel = FitSkinToModel(a_nifFile, a_shape)) {
            UseSkinnedVertices(a_geometry, PlaceVertices(a_geometry.raw, *skinToModel));
            return true;
        }
    }

    const ShapeSkin skin(a_nifFile, a_shape);
    if (!skin.HasUsableBones()) {
        ReportMeshProblem(QStringLiteral("Skipping skinning for NIF shape '%1': missing bone transforms")
                .arg(QString::fromStdString(ShapeName(a_shape))));
        return false;
    }
    if (!skin.HasInfluences()) {
        ReportMeshProblem(QStringLiteral("Skipping skinning for NIF shape '%1': no usable skin weights")
                .arg(QString::fromStdString(ShapeName(a_shape))));
        return false;
    }
    if (const auto unweighted = skin.UnweightedVertexCount(); unweighted > 0) {
        ReportMeshProblem(QStringLiteral("NIF shape '%1' has %2 vertices without usable skin weights")
                .arg(QString::fromStdString(ShapeName(a_shape)))
                .arg(unweighted));
    }

    UseSkinnedVertices(a_geometry, skin.AtRest(a_geometry.raw));
    a_geometry.triangles = GetShapeTriangles(a_shape, skin.WeightPartition());
    return true;
}
} // namespace

const std::vector<nifly::Vector3>* ShapeRenderGeometry::Positions() const {
    return skinned ? &deformed.positions : raw.positions;
}

const std::vector<nifly::Vector3>* ShapeRenderGeometry::Normals() const {
    return skinned ? VectorOrNull(deformed.normals) : raw.normals;
}

const std::vector<nifly::Vector3>* ShapeRenderGeometry::Tangents() const {
    return skinned ? VectorOrNull(deformed.tangents) : raw.tangents;
}

const std::vector<nifly::Vector3>* ShapeRenderGeometry::Bitangents() const {
    return skinned ? VectorOrNull(deformed.bitangents) : raw.bitangents;
}

void ValidateShapeGeometry(nifly::NiShape* a_shape) {
    if (auto* const geomData = a_shape->GetGeomData()) {
        if (!a_shape->HasUVs()) {
            a_shape->SetUVs(true);
        }
        if (!a_shape->HasNormals()) {
            a_shape->SetNormals(true);
            geomData->RecalcNormals();
        }
        if (!a_shape->HasTangents() || geomData->tangents.empty()) {
            a_shape->SetTangents(true);
            geomData->CalcTangentSpace();
        }
        if (!a_shape->HasVertexColors()) {
            a_shape->SetVertexColors(true);
        }
    }
}

ShapeRenderGeometry PrepareShapeRenderGeometry(nifly::NifFile* a_nifFile, nifly::NiShape* a_shape) {
    ShapeRenderGeometry geometry;
    geometry.raw = SkinSourceOf(a_nifFile, a_shape);
    geometry.modelTransform = GetShapeTransformToGlobal(a_nifFile, a_shape);
    geometry.bounds = GetBoundingSphere(a_nifFile, a_shape);
    geometry.triangles = GetShapeTriangles(a_shape, nullptr);

    TryBuildSkinnedGeometry(a_nifFile, a_shape, geometry);
    if (const auto* const positions = geometry.Positions()) {
        for (const auto& position : *positions) {
            geometry.box.Add(geometry.modelTransform.ApplyTransform(position));
        }
    }
    return geometry;
}
