#include "OpenGLShape.h"
#include "NifExtensions.h"

#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLFunctions_2_1>
#include <QOpenGLVersionFunctionsFactory>

#include <Skin.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>

namespace
{
constexpr float MinSkinWeight = 0.000001f;

struct BoneTransform
{
  nifly::MatTransform transform;
  bool valid = false;
};

struct RenderGeometry
{
  const std::vector<nifly::Vector3>* rawPositions  = nullptr;
  const std::vector<nifly::Vector3>* rawNormals    = nullptr;
  const std::vector<nifly::Vector3>* rawTangents   = nullptr;
  const std::vector<nifly::Vector3>* rawBitangents = nullptr;

  std::vector<nifly::Vector3> skinnedPositions;
  std::vector<nifly::Vector3> skinnedNormals;
  std::vector<nifly::Vector3> skinnedTangents;
  std::vector<nifly::Vector3> skinnedBitangents;
  std::vector<nifly::Triangle> triangles;

  nifly::MatTransform modelTransform;
  nifly::BoundingSphere bounds;
  bool skinned = false;

  [[nodiscard]] const std::vector<nifly::Vector3>* positions() const
  {
    return skinned ? &skinnedPositions : rawPositions;
  }

  [[nodiscard]] const std::vector<nifly::Vector3>* normals() const
  {
    return skinned ? vectorOrNull(skinnedNormals) : rawNormals;
  }

  [[nodiscard]] const std::vector<nifly::Vector3>* tangents() const
  {
    return skinned ? vectorOrNull(skinnedTangents) : rawTangents;
  }

  [[nodiscard]] const std::vector<nifly::Vector3>* bitangents() const
  {
    return skinned ? vectorOrNull(skinnedBitangents) : rawBitangents;
  }

private:
  static const std::vector<nifly::Vector3>*
  vectorOrNull(const std::vector<nifly::Vector3>& values)
  {
    return values.empty() ? nullptr : &values;
  }
};

std::string shapeName(nifly::NiShape* shape)
{
  const auto name = shape->name.get();
  return name.empty() ? std::string("<unnamed>") : name;
}

std::uint32_t blockRefIndex(const nifly::NiBlockRef<nifly::NiNode>& ref)
{
  return ref.index;
}

bool isLegacyTriShape(const nifly::NiShape* shape)
{
  return shape->HasType<nifly::NiTriShape>() || shape->HasType<nifly::NiTriStrips>();
}

bool hasSameSize(const std::vector<nifly::Vector3>* values,
                 const std::vector<nifly::Vector3>* positions)
{
  return values && positions && values->size() == positions->size();
}

float partitionWeight(const nifly::VertexWeight& weights, const std::size_t slot)
{
  switch (slot) {
  case 0:
    return weights.w1;
  case 1:
    return weights.w2;
  case 2:
    return weights.w3;
  case 3:
    return weights.w4;
  default:
    return 0.0f;
  }
}

std::uint8_t partitionBoneIndex(const nifly::BoneIndices& indices,
                                const std::size_t slot)
{
  switch (slot) {
  case 0:
    return indices.i1;
  case 1:
    return indices.i2;
  case 2:
    return indices.i3;
  case 3:
    return indices.i4;
  default:
    return 0;
  }
}

void normalizeVectors(std::vector<nifly::Vector3>& values)
{
  for (auto& value : values) {
    if (!value.IsZero(true)) {
      value.Normalize();
    }
  }
}

void transformDirection(const std::vector<nifly::Vector3>* source,
                        std::vector<nifly::Vector3>& destination,
                        const std::size_t vertex, const nifly::MatTransform& transform,
                        const float weight)
{
  if (source && vertex < source->size() && vertex < destination.size()) {
    destination[vertex] += transform.ApplyTransformToDir((*source)[vertex]) * weight;
  }
}

bool applySkinWeight(const RenderGeometry& geometry, const std::size_t vertex,
                     const float weight, const nifly::MatTransform& transform,
                     RenderGeometry& skinnedGeometry,
                     std::vector<char>& weightedVertices)
{
  if (!geometry.rawPositions || vertex >= geometry.rawPositions->size() ||
      weight <= MinSkinWeight) {
    return false;
  }

  skinnedGeometry.skinnedPositions[vertex] +=
      transform.ApplyTransform((*geometry.rawPositions)[vertex]) * weight;
  transformDirection(geometry.rawNormals, skinnedGeometry.skinnedNormals, vertex,
                     transform, weight);
  transformDirection(geometry.rawTangents, skinnedGeometry.skinnedTangents, vertex,
                     transform, weight);
  transformDirection(geometry.rawBitangents, skinnedGeometry.skinnedBitangents, vertex,
                     transform, weight);
  weightedVertices[vertex] = true;
  return true;
}

bool getNodeTransformToGlobal(const nifly::NifFile* nifFile, const std::uint32_t nodeId,
                              nifly::MatTransform& transform)
{
  const auto node = nifFile->GetHeader().GetBlock<nifly::NiNode>(nodeId);
  if (!node) {
    return false;
  }

  transform = GetObjectTransformToGlobal(nifFile, node);
  return true;
}

bool hasPartitionWeights(const nifly::NiSkinPartition* skinPartition)
{
  if (!skinPartition) {
    return false;
  }

  return std::ranges::any_of(
      skinPartition->partitions,
      [](const nifly::NiSkinPartition::PartitionBlock& partition) {
        return partition.numWeightsPerVertex > 0 && !partition.bones.empty() &&
               !partition.vertexWeights.empty() && !partition.boneIndices.empty();
      });
}

std::vector<nifly::BoneIndices>::size_type
partitionVertexLimit(const nifly::NiSkinPartition::PartitionBlock& partition)
{
  auto limit = partition.boneIndices.size();
  limit      = std::min(limit, partition.vertexWeights.size());
  if (partition.hasVertexMap) {
    limit = std::min(limit, partition.vertexMap.size());
  }
  if (partition.numVertices > 0) {
    limit = std::min(limit, static_cast<std::size_t>(partition.numVertices));
  }
  return limit;
}

std::vector<nifly::Triangle> getShapeTriangles(const nifly::NiShape* shape,
                                               nifly::NiSkinPartition* skinPartition)
{
  std::vector<nifly::Triangle> triangles;

  if (skinPartition) {
    skinPartition->PrepareTrueTriangles();
    for (const auto& partition : skinPartition->partitions) {
      triangles.insert(triangles.end(), partition.trueTriangles.begin(),
                       partition.trueTriangles.end());
    }
    if (!triangles.empty()) {
      return triangles;
    }
  }

  shape->GetTriangles(triangles);
  return triangles;
}

std::vector<BoneTransform> getBoneTransforms(nifly::NifFile* nifFile,
                                             nifly::NiShape* shape,
                                             const bool legacySkinDataMode)
{
  const auto& header = nifFile->GetHeader();
  const auto skinContainer =
      header.GetBlock<nifly::NiBoneContainer>(shape->SkinInstanceRef());
  if (!skinContainer) {
    return {};
  }

  const auto shapeToGlobal = GetShapeTransformToGlobal(nifFile, shape);
  nifly::MatTransform globalToSkin;
  const auto hasGlobalToSkin =
      nifFile->GetShapeTransformGlobalToSkin(shape, globalToSkin);

  const auto skinInstance = dynamic_cast<nifly::NiSkinInstance*>(skinContainer);
  const auto skeletonRootId =
      skinInstance ? blockRefIndex(skinInstance->targetRef) : nifly::NIF_NPOS;

  std::vector<BoneTransform> transforms(skinContainer->boneRefs.GetSize());
  for (std::uint32_t boneIndex = 0; boneIndex < skinContainer->boneRefs.GetSize();
       boneIndex++) {
    const auto boneId = skinContainer->boneRefs.GetBlockRef(boneIndex);
    if (boneId == nifly::NIF_NPOS) {
      continue;
    }

    nifly::MatTransform skinToBone;
    if (!nifFile->GetShapeTransformSkinToBone(shape, boneIndex, skinToBone)) {
      continue;
    }

    const auto bone = header.GetBlock<nifly::NiNode>(boneId);
    if (!bone) {
      continue;
    }

    if (legacySkinDataMode && hasGlobalToSkin && skeletonRootId != nifly::NIF_NPOS) {
      nifly::MatTransform boneToSkeletonRoot;
      if (GetNodeTransformToAncestor(nifFile, bone, skeletonRootId,
                                     boneToSkeletonRoot)) {
        transforms[boneIndex].transform = shapeToGlobal.ComposeTransforms(globalToSkin)
                                              .ComposeTransforms(boneToSkeletonRoot)
                                              .ComposeTransforms(skinToBone);
        transforms[boneIndex].valid = true;
        continue;
      }
    }

    nifly::MatTransform boneToGlobal;
    if (getNodeTransformToGlobal(nifFile, boneId, boneToGlobal)) {
      transforms[boneIndex].transform = boneToGlobal.ComposeTransforms(skinToBone);
      transforms[boneIndex].valid     = true;
    }
  }

  return transforms;
}

bool skinFromPartitions(const nifly::NiSkinPartition* skinPartition,
                        const std::vector<BoneTransform>& boneTransforms,
                        const RenderGeometry& geometry, RenderGeometry& skinnedGeometry,
                        std::vector<char>& weightedVertices)
{
  bool skinnedAnyVertex = false;

  for (const auto& partition : skinPartition->partitions) {
    const auto weightCount = std::min<std::size_t>(partition.numWeightsPerVertex, 4);
    if (weightCount == 0 || partition.bones.empty()) {
      continue;
    }

    const auto vertexLimit = partitionVertexLimit(partition);
    for (std::size_t partitionVertex = 0; partitionVertex < vertexLimit;
         partitionVertex++) {
      const auto shapeVertex = partition.hasVertexMap
                                   ? partition.vertexMap[partitionVertex]
                                   : static_cast<std::uint16_t>(partitionVertex);
      if (shapeVertex >= weightedVertices.size() || weightedVertices[shapeVertex]) {
        continue;
      }

      bool skinnedVertex = false;
      for (std::size_t weightSlot = 0; weightSlot < weightCount; weightSlot++) {
        const auto localBoneIndex =
            partitionBoneIndex(partition.boneIndices[partitionVertex], weightSlot);
        if (localBoneIndex >= partition.bones.size()) {
          continue;
        }

        const auto boneIndex = partition.bones[localBoneIndex];
        if (boneIndex >= boneTransforms.size() || !boneTransforms[boneIndex].valid) {
          continue;
        }

        const auto weight =
            partitionWeight(partition.vertexWeights[partitionVertex], weightSlot);
        skinnedVertex |= applySkinWeight(geometry, shapeVertex, weight,
                                         boneTransforms[boneIndex].transform,
                                         skinnedGeometry, weightedVertices);
      }

      skinnedAnyVertex |= skinnedVertex;
    }
  }

  return skinnedAnyVertex;
}

bool skinFromBoneWeights(const nifly::NifFile* nifFile, nifly::NiShape* shape,
                         const std::vector<BoneTransform>& boneTransforms,
                         const RenderGeometry& geometry,
                         RenderGeometry& skinnedGeometry,
                         std::vector<char>& weightedVertices)
{
  bool skinnedAnyVertex = false;

  for (std::uint32_t boneIndex = 0; boneIndex < boneTransforms.size(); boneIndex++) {
    if (!boneTransforms[boneIndex].valid) {
      continue;
    }

    std::unordered_map<std::uint16_t, float> weights;
    if (nifFile->GetShapeBoneWeights(shape, boneIndex, weights) == 0) {
      continue;
    }

    for (const auto& [vertex, weight] : weights) {
      skinnedAnyVertex |=
          applySkinWeight(geometry, vertex, weight, boneTransforms[boneIndex].transform,
                          skinnedGeometry, weightedVertices);
    }
  }

  return skinnedAnyVertex;
}

void copyRigidVertex(const RenderGeometry& source, const std::size_t vertex,
                     const nifly::MatTransform& transform, RenderGeometry& destination)
{
  destination.skinnedPositions[vertex] =
      transform.ApplyTransform((*source.rawPositions)[vertex]);

  if (!destination.skinnedNormals.empty() && source.rawNormals &&
      vertex < source.rawNormals->size()) {
    destination.skinnedNormals[vertex] =
        transform.ApplyTransformToDir((*source.rawNormals)[vertex]);
  }
  if (!destination.skinnedTangents.empty() && source.rawTangents &&
      vertex < source.rawTangents->size()) {
    destination.skinnedTangents[vertex] =
        transform.ApplyTransformToDir((*source.rawTangents)[vertex]);
  }
  if (!destination.skinnedBitangents.empty() && source.rawBitangents &&
      vertex < source.rawBitangents->size()) {
    destination.skinnedBitangents[vertex] =
        transform.ApplyTransformToDir((*source.rawBitangents)[vertex]);
  }
}

bool tryBuildSkinnedGeometry(nifly::NifFile* nifFile, nifly::NiShape* shape,
                             RenderGeometry& geometry)
{
  if (!shape->HasSkinInstance() || !geometry.rawPositions ||
      geometry.rawPositions->empty()) {
    return false;
  }

  const auto& header = nifFile->GetHeader();
  const auto skinContainer =
      header.GetBlock<nifly::NiBoneContainer>(shape->SkinInstanceRef());
  if (!skinContainer) {
    qWarning("Skipping skinning for NIF shape '%s': missing skin instance",
             shapeName(shape).c_str());
    return false;
  }

  const auto skinInstance = dynamic_cast<nifly::NiSkinInstance*>(skinContainer);
  auto skinPartition =
      skinInstance ? header.GetBlock(skinInstance->skinPartitionRef) : nullptr;

  const bool useLegacyPartition =
      isLegacyTriShape(shape) && hasPartitionWeights(skinPartition);
  auto boneTransforms =
      getBoneTransforms(nifFile, shape, isLegacyTriShape(shape) && !useLegacyPartition);
  if (boneTransforms.empty() ||
      std::ranges::none_of(boneTransforms, [](const BoneTransform& transform) {
        return transform.valid;
      })) {
    qWarning("Skipping skinning for NIF shape '%s': missing bone transforms",
             shapeName(shape).c_str());
    return false;
  }

  RenderGeometry skinnedGeometry = geometry;
  skinnedGeometry.skinnedPositions.assign(geometry.rawPositions->size(), {});
  if (hasSameSize(geometry.rawNormals, geometry.rawPositions)) {
    skinnedGeometry.skinnedNormals.assign(geometry.rawPositions->size(), {});
  }
  if (hasSameSize(geometry.rawTangents, geometry.rawPositions)) {
    skinnedGeometry.skinnedTangents.assign(geometry.rawPositions->size(), {});
  }
  if (hasSameSize(geometry.rawBitangents, geometry.rawPositions)) {
    skinnedGeometry.skinnedBitangents.assign(geometry.rawPositions->size(), {});
  }

  std::vector<char> weightedVertices(geometry.rawPositions->size(), false);
  const auto skinnedAnyVertex =
      useLegacyPartition ? skinFromPartitions(skinPartition, boneTransforms, geometry,
                                              skinnedGeometry, weightedVertices)
                         : skinFromBoneWeights(nifFile, shape, boneTransforms, geometry,
                                               skinnedGeometry, weightedVertices);

  if (!skinnedAnyVertex) {
    qWarning("Skipping skinning for NIF shape '%s': no usable skin weights",
             shapeName(shape).c_str());
    return false;
  }

  const auto rigidTransform = GetShapeTransformToGlobal(nifFile, shape);
  const auto rigidVertices =
      static_cast<std::size_t>(std::ranges::count(weightedVertices, 0));
  if (rigidVertices > 0) {
    qWarning("NIF shape '%s' has %zu vertices without usable skin weights",
             shapeName(shape).c_str(), rigidVertices);
  }
  for (std::size_t vertex = 0; vertex < weightedVertices.size(); vertex++) {
    if (!weightedVertices[vertex]) {
      copyRigidVertex(geometry, vertex, rigidTransform, skinnedGeometry);
    }
  }

  normalizeVectors(skinnedGeometry.skinnedNormals);
  normalizeVectors(skinnedGeometry.skinnedTangents);
  normalizeVectors(skinnedGeometry.skinnedBitangents);

  skinnedGeometry.skinned = true;
  skinnedGeometry.bounds  = nifly::BoundingSphere(skinnedGeometry.skinnedPositions);
  skinnedGeometry.triangles =
      getShapeTriangles(shape, useLegacyPartition ? skinPartition : nullptr);
  skinnedGeometry.modelTransform.Clear();

  geometry = std::move(skinnedGeometry);
  return true;
}

RenderGeometry prepareRenderGeometry(nifly::NifFile* nifFile, nifly::NiShape* shape)
{
  RenderGeometry geometry;
  geometry.rawPositions   = nifFile->GetVertsForShape(shape);
  geometry.rawNormals     = nifFile->GetNormalsForShape(shape);
  geometry.rawTangents    = nifFile->GetTangentsForShape(shape);
  geometry.rawBitangents  = nifFile->GetBitangentsForShape(shape);
  geometry.modelTransform = GetShapeTransformToGlobal(nifFile, shape);
  geometry.bounds         = GetBoundingSphere(nifFile, shape);
  geometry.triangles      = getShapeTriangles(shape, nullptr);

  tryBuildSkinnedGeometry(nifFile, shape, geometry);
  return geometry;
}
}  // namespace

template <typename T>
static QOpenGLBuffer* makeVertexBuffer(const std::vector<T>* data, const GLuint attrib)
{
  QOpenGLBuffer* buffer = nullptr;

  if (data && !data->empty()) {
    const auto byteSize = data->size() * sizeof(T);
    if (byteSize > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
      qWarning("Skipping oversized vertex buffer for attribute %u", attrib);
      return nullptr;
    }

    buffer = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    if (buffer->create() && buffer->bind()) {
      buffer->allocate(data->data(), static_cast<int>(byteSize));

      const auto f = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_2_1>(
          QOpenGLContext::currentContext());
      if (!f) {
        qWarning("Skipping vertex attribute setup: OpenGL 2.1 functions unavailable");
        buffer->release();
        return buffer;
      }

      f->glEnableVertexAttribArray(attrib);

      f->glVertexAttribPointer(attrib, sizeof(T) / sizeof(float), GL_FLOAT, GL_FALSE,
                               sizeof(T), nullptr);

      buffer->release();
    }
  }

  return buffer;
}

void validateShapeGeometry(nifly::NiShape* shape)
{
  if (const auto geomData = shape->GetGeomData()) {
    if (!shape->HasUVs()) {
      shape->SetUVs(true);
    }
    if (!shape->HasNormals()) {
      shape->SetNormals(true);
      geomData->RecalcNormals();
    }
    if (!shape->HasTangents() || geomData->tangents.empty()) {
      shape->SetTangents(true);
      geomData->CalcTangentSpace();
    }
    if (!shape->HasVertexColors()) {
      shape->SetVertexColors(true);
    }
  }
}

OpenGLShape::OpenGLShape(nifly::NifFile* nifFile, nifly::NiShape* niShape,
                         TextureManager* textureManager)
{
  const auto f = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_2_1>(
      QOpenGLContext::currentContext());
  if (!f) {
    qWarning("Skipping NIF shape: OpenGL 2.1 functions unavailable");
    return;
  }

  const auto shader = nifFile->GetShader(niShape);

  if (const auto& version = nifFile->GetHeader().GetVersion();
      shader && version.IsFO4()) {
    shaderType = shader->HasType<nifly::BSEffectShaderProperty>()
                     ? ShaderManager::FO4EffectShader
                     : ShaderManager::FO4Default;
  } else if (shader) {
    if (shader->HasType<nifly::BSEffectShaderProperty>()) {
      shaderType = ShaderManager::SKEffectShader;
    } else if (shader->IsModelSpace()) {
      shaderType = ShaderManager::SKMSN;
    } else if (shader->GetShaderType() == nifly::BSLSP_MULTILAYERPARALLAX) {
      shaderType = ShaderManager::SKMultilayer;
    } else if (const auto bslsp =
                   dynamic_cast<nifly::BSLightingShaderProperty*>(shader)) {
      shaderType = (bslsp->shaderFlags2 & SLSF2::PBR) ? ShaderManager::SKPBR
                                                      : ShaderManager::SKDefault;
    } else {
      shaderType = ShaderManager::SKDefault;
    }
  } else {
    shaderType = ShaderManager::None;
  }

  vertexArray = new QOpenGLVertexArrayObject();
  vertexArray->create();
  auto binder = QOpenGLVertexArrayObject::Binder(vertexArray);

  f->glVertexAttrib2f(AttribTexCoord, 0.0f, 0.0f);
  f->glVertexAttrib4f(AttribColor, 1.0f, 1.0f, 1.0f, 1.0f);

  validateShapeGeometry(niShape);

  const auto geometry = prepareRenderGeometry(nifFile, niShape);
  modelMatrix         = convertTransform(geometry.modelTransform);
  bounds              = geometry.bounds;

  if (const auto verts = geometry.positions()) {
    vertexBuffers[AttribPosition] = makeVertexBuffer(verts, AttribPosition);
  }

  if (const auto normals = geometry.normals()) {
    vertexBuffers[AttribNormal] = makeVertexBuffer(normals, AttribNormal);
  }

  if (const auto tangents = geometry.tangents()) {
    vertexBuffers[AttribTangent] = makeVertexBuffer(tangents, AttribTangent);
  }

  if (const auto bitangents = geometry.bitangents()) {
    vertexBuffers[AttribBitangent] = makeVertexBuffer(bitangents, AttribBitangent);
  }

  if (const auto uvs = nifFile->GetUvsForShape(niShape)) {
    vertexBuffers[AttribTexCoord] = makeVertexBuffer(uvs, AttribTexCoord);
  }

  if (std::vector<nifly::Color4> colors; nifFile->GetColorsForShape(niShape, colors)) {
    if (const auto bslsp = dynamic_cast<nifly::BSLightingShaderProperty*>(shader)) {
      if (!(bslsp->shaderFlags1 & SLSF1::VertexAlpha) ||
          bslsp->shaderFlags2 & SLSF2::TreeAnim) {
        for (auto& color : colors) {
          color.a = 1.0f;
        }
      }
    }

    vertexBuffers[AttribColor] = makeVertexBuffer(&colors, AttribColor);
  }

  indexBuffer = new QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
  if (indexBuffer->create() && indexBuffer->bind()) {

    if (!geometry.triangles.empty()) {
      const auto byteSize = geometry.triangles.size() * sizeof(nifly::Triangle);
      if (byteSize <= static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        indexBuffer->allocate(geometry.triangles.data(), static_cast<int>(byteSize));
      } else {
        qWarning("Skipping oversized index buffer");
      }
    }

    const auto iElements = static_cast<std::uint32_t>(std::min<std::size_t>(
        geometry.triangles.size() * 3, std::numeric_limits<std::uint32_t>::max()));
    elements             = static_cast<GLsizei>(std::min(
        iElements, static_cast<uint32_t>(std::numeric_limits<GLsizei>::max())));
    indexBuffer->release();
  }

  if (shader) {
    if (shader->HasTextureSet()) {
      const auto textureSetRef = shader->TextureSetRef();
      const auto textureSet    = nifFile->GetHeader().GetBlock(textureSetRef);

      if (!textureSet) {
        qWarning("Skipping missing shader texture set");
      }

      const auto nifTextureCount =
          textureSet ? static_cast<std::size_t>(textureSet->textures.size()) : 0;
      const auto textureCount = std::min(nifTextureCount, textures.size());
      if (textureSet && nifTextureCount > textures.size()) {
        qWarning("Skipping %zu unsupported texture slots",
                 nifTextureCount - textures.size());
      }

      for (std::size_t i = 0; i < textureCount; i++) {
        const auto textureIndex = static_cast<std::uint32_t>(i);
        if (auto texturePath = textureSet->textures[textureIndex].get();
            !texturePath.empty()) {
          textures[i] = textureManager->getTexture(texturePath);
        }

        if (textures[i] == nullptr) {
          switch (i) {
          case TextureSlot::BaseMap:
            textures[i] = textureManager->getErrorTexture();
            break;
          case TextureSlot::NormalMap:
            textures[i] = textureManager->getFlatNormalTexture();
            break;
          case TextureSlot::GlowMap:
            if (shader->HasGlowmap()) {
              textures[i] = textureManager->getBlackTexture();
            } else {
              textures[i] = textureManager->getWhiteTexture();
            }
            break;
          default:
            textures[i] = nullptr;
            break;
          }
        }
      }
    }

    specColor      = convertVector3(shader->GetSpecularColor());
    specStrength   = shader->GetSpecularStrength();
    specGlossiness = qBound(0.0f, shader->GetGlossiness(), 128.0f);
    fresnelPower   = shader->GetFresnelPower();
    paletteScale   = shader->GetGrayscaleToPaletteScale();

    hasGlowMap = shader->HasGlowmap();
    glowColor  = convertColor(shader->GetEmissiveColor());
    glowMult   = shader->GetEmissiveMultiple();

    alpha    = shader->GetAlpha();
    uvScale  = convertVector2(shader->GetUVScale());
    uvOffset = convertVector2(shader->GetUVOffset());

    hasEmit      = shader->IsEmissive();
    hasSoftlight = shader->HasSoftlight();
    hasBacklight = shader->HasBacklight();
    hasRimlight  = shader->HasRimlight();

    softlight      = shader->GetSoftlight();
    backlightPower = shader->GetBacklightPower();
    rimPower       = shader->GetRimlightPower();
    doubleSided    = shader->IsDoubleSided();
    envReflection  = shader->GetEnvironmentMapScale();

    if (const auto alphaProperty = nifFile->GetAlphaProperty(niShape)) {

      const NiAlphaPropertyFlags flags = alphaProperty->flags;

      alphaBlendEnable = flags.isAlphaBlendEnabled();
      srcBlendMode     = flags.sourceBlendingFactor();
      dstBlendMode     = flags.destinationBlendingFactor();
      alphaTestEnable  = flags.isAlphaTestEnabled();
      alphaTestMode    = flags.alphaTestMode();

      alphaThreshold = static_cast<float>(alphaProperty->threshold) / 255.0f;
    }

    if (const auto bslsp = dynamic_cast<nifly::BSLightingShaderProperty*>(shader)) {
      zBufferTest  = bslsp->shaderFlags1 & SLSF1::ZBufferTest;
      zBufferWrite = bslsp->shaderFlags2 & SLSF2::ZBufferWrite;

      const auto bslspType = bslsp->GetShaderType();
      if (bslspType == nifly::BSLSP_SKINTINT || bslspType == nifly::BSLSP_FACE) {
        tintColor    = convertVector3(bslsp->skinTintColor);
        hasTintColor = true;
      } else if (bslspType == nifly::BSLSP_HAIRTINT) {
        tintColor    = convertVector3(bslsp->hairTintColor);
        hasTintColor = true;
      }

      if (bslspType == nifly::BSLSP_MULTILAYERPARALLAX) {
        innerScale      = convertVector2(bslsp->parallaxInnerLayerTextureScale);
        innerThickness  = bslsp->parallaxInnerLayerThickness;
        outerRefraction = bslsp->parallaxRefractionScale;
        outerReflection = bslsp->parallaxEnvmapStrength;
      }
    }

    if (const auto effectShader =
            dynamic_cast<nifly::BSEffectShaderProperty*>(shader)) {
      hasWeaponBlood = effectShader->shaderFlags2 & SLSF2::WeaponBlood;
    }
  } else {
    textures[BaseMap]   = textureManager->getWhiteTexture();
    textures[NormalMap] = textureManager->getFlatNormalTexture();
  }
}

void OpenGLShape::destroy()
{
  for (auto& vertexBuffer : vertexBuffers) {
    if (vertexBuffer) {
      vertexBuffer->destroy();
      delete vertexBuffer;
      vertexBuffer = nullptr;
    }
  }

  if (indexBuffer) {
    indexBuffer->destroy();
    delete indexBuffer;
    indexBuffer = nullptr;
  }

  if (vertexArray) {
    vertexArray->destroy();
    vertexArray->deleteLater();
  }
}

void OpenGLShape::setupShaders(QOpenGLShaderProgram* program) const
{
  program->setUniformValue("BaseMap", BaseMap + 1);
  program->setUniformValue("NormalMap", NormalMap + 1);
  program->setUniformValue("GlowMap", GlowMap + 1);
  program->setUniformValue("LightMask", LightMask + 1);
  program->setUniformValue("hasGlowMap", hasGlowMap && textures[GlowMap] != nullptr);
  program->setUniformValue("HeightMap", HeightMap + 1);
  program->setUniformValue("hasHeightMap", textures[HeightMap] != nullptr);
  program->setUniformValue("DetailMask", DetailMask + 1);
  program->setUniformValue("hasDetailMask", textures[DetailMask] != nullptr);
  program->setUniformValue("CubeMap", EnvironmentMap + 1);
  program->setUniformValue("hasCubeMap", textures[EnvironmentMap] != nullptr);
  program->setUniformValue("EnvironmentMap", EnvironmentMask + 1);
  program->setUniformValue("hasEnvMask", textures[EnvironmentMask] != nullptr);
  program->setUniformValue("TintMask", TintMask + 1);
  program->setUniformValue("hasTintMask", textures[TintMask] != nullptr);
  program->setUniformValue("InnerMap", InnerMap + 1);
  program->setUniformValue("BacklightMap", BacklightMap + 1);
  program->setUniformValue("SpecularMap", SpecularMap + 1);
  program->setUniformValue("hasSpecularMap", textures[SpecularMap] != nullptr);

  for (int i = 0; i < textures.size(); i++) {
    if (textures[i]) {
      textures[i]->bind(i + 1);
    }
  }

  program->setUniformValue("ambientColor", QVector4D(0.2f, 0.2f, 0.2f, 1.0f));
  program->setUniformValue("diffuseColor", QVector4D(1.0f, 1.0f, 1.0f, 1.0f));

  program->setUniformValue("alpha", alpha);
  program->setUniformValue("alphaThreshold", alphaThreshold);
  program->setUniformValue("tintColor", tintColor);
  program->setUniformValue("uvScale", uvScale);
  program->setUniformValue("uvOffset", uvOffset);
  program->setUniformValue("specColor", specColor);
  program->setUniformValue("specStrength", specStrength);
  program->setUniformValue("specGlossiness", specGlossiness);
  program->setUniformValue("fresnelPower", fresnelPower);

  program->setUniformValue("paletteScale", paletteScale);

  program->setUniformValue("hasEmit", hasEmit);
  program->setUniformValue("hasSoftlight", hasSoftlight);
  program->setUniformValue("hasBacklight", hasBacklight);
  program->setUniformValue("hasRimlight", hasRimlight);
  program->setUniformValue("hasTintColor", hasTintColor);
  program->setUniformValue("hasWeaponBlood", hasWeaponBlood);

  program->setUniformValue("softlight", softlight);
  program->setUniformValue("backlightPower", backlightPower);
  program->setUniformValue("rimPower", rimPower);
  program->setUniformValue("subsurfaceRolloff", subsurfaceRolloff);
  program->setUniformValue("doubleSided", doubleSided);

  program->setUniformValue("envReflection", envReflection);

  if (shaderType == ShaderManager::SKMultilayer) {
    program->setUniformValue("innerScale", innerScale);
    program->setUniformValue("innerThickness", innerThickness);
    program->setUniformValue("outerRefraction", outerRefraction);
    program->setUniformValue("outerReflection", outerReflection);
  }

  const auto f = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_2_1>(
      QOpenGLContext::currentContext());

  for (std::size_t i = 0; i < ATTRIB_COUNT; i++) {
    if (vertexBuffers[i]) {
      f->glEnableVertexAttribArray(static_cast<GLuint>(i));
    } else {
      f->glDisableVertexAttribArray(static_cast<GLuint>(i));
    }
  }

  f->glDepthMask(zBufferWrite ? GL_TRUE : GL_FALSE);

  if (zBufferTest) {
    f->glEnable(GL_DEPTH_TEST);
    f->glDepthFunc(GL_LEQUAL);
  } else {
    f->glDisable(GL_DEPTH_TEST);
  }

  if (doubleSided) {
    f->glDisable(GL_CULL_FACE);
  } else {
    f->glEnable(GL_CULL_FACE);
    f->glCullFace(GL_BACK);
  }

  if (alphaBlendEnable) {
    f->glDisable(GL_POLYGON_OFFSET_FILL);
    f->glEnable(GL_BLEND);
    f->glBlendFunc(srcBlendMode, dstBlendMode);
  } else {
    f->glDisable(GL_BLEND);
  }

  if (alphaTestEnable) {
    f->glDisable(GL_ALPHA_TEST);
  }
}

QVector2D OpenGLShape::convertVector2(nifly::Vector2 vector)
{
  return {vector.u, vector.v};
}

QVector3D OpenGLShape::convertVector3(nifly::Vector3 vector)
{
  return {vector.x, vector.y, vector.z};
}

QColor OpenGLShape::convertColor(const nifly::Color4 color)
{
  return QColor::fromRgbF(color.r, color.g, color.b, color.a);
}

QMatrix4x4 OpenGLShape::convertTransform(const nifly::MatTransform& transform)
{
  auto mat = transform.ToMatrix();
  return QMatrix4x4{
      mat[0], mat[1], mat[2],  mat[3],  mat[4],  mat[5],  mat[6],  mat[7],
      mat[8], mat[9], mat[10], mat[11], mat[12], mat[13], mat[14], mat[15],
  };
}
