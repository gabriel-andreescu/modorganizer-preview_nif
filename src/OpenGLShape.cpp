#include "OpenGLShape.h"
#include "NifExtensions.h"

#include <QOpenGLContext>
#include <QOpenGLFunctions_2_1>
#include <QOpenGLVersionFunctionsFactory>

template <typename T>
static QOpenGLBuffer* makeVertexBuffer(const std::vector<T>* data, const GLuint attrib)
{
  QOpenGLBuffer* buffer = nullptr;

  if (data) {
    buffer = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    if (buffer->create() && buffer->bind()) {
      buffer->allocate(data->data(), data->size() * sizeof(T));

      const auto f = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_2_1>(
          QOpenGLContext::currentContext());

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
    if (!shape->HasUVs()) { shape->SetUVs(true); }
    if (!shape->HasNormals()) {
      shape->SetNormals(true);
      geomData->RecalcNormals();
    }
    if (!shape->HasTangents() || geomData->tangents.empty()) {
      shape->SetTangents(true);
      geomData->CalcTangentSpace();
    }
    if (!shape->HasVertexColors()) { shape->SetVertexColors(true); }
  }
}

OpenGLShape::OpenGLShape(nifly::NifFile* nifFile, nifly::NiShape* niShape,
                         TextureManager* textureManager)
{
  const auto f = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_2_1>(
      QOpenGLContext::currentContext());

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
    } else if (
      const auto bslsp = dynamic_cast<nifly::BSLightingShaderProperty*>(shader)
    ) {
      shaderType = (bslsp->shaderFlags2 & SLSF2::PBR)
                     ? ShaderManager::SKPBR
                     : ShaderManager::SKDefault;
    } else {
      shaderType = ShaderManager::SKDefault;
    }
  }

  vertexArray = new QOpenGLVertexArrayObject();
  vertexArray->create();
  auto binder = QOpenGLVertexArrayObject::Binder(vertexArray);

  const auto xform = GetShapeTransformToGlobal(nifFile, niShape);
  modelMatrix      = convertTransform(xform);

  f->glVertexAttrib2f(AttribTexCoord, 0.0f, 0.0f);
  f->glVertexAttrib4f(AttribColor, 1.0f, 1.0f, 1.0f, 1.0f);

  validateShapeGeometry(niShape);

  if (const auto verts = nifFile->GetVertsForShape(niShape)) {
    vertexBuffers[AttribPosition] = makeVertexBuffer(verts, AttribPosition);
  }

  if (const auto normals = nifFile->GetNormalsForShape(niShape)) {
    vertexBuffers[AttribNormal] = makeVertexBuffer(normals, AttribNormal);
  }

  if (const auto tangents = nifFile->GetTangentsForShape(niShape)) {
    vertexBuffers[AttribTangent] = makeVertexBuffer(tangents, AttribTangent);
  }

  if (const auto bitangents = nifFile->GetBitangentsForShape(niShape)) {
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

    if (std::vector<nifly::Triangle> tris; niShape->GetTriangles(tris)) {
      indexBuffer->allocate(tris.data(),
                            static_cast<int>(tris.size() * sizeof(nifly::Triangle)));
    }

    const uint32_t iElements = niShape->GetNumTriangles() * 3;
    elements                 = static_cast<GLsizei>(
      std::min(
          iElements,
          static_cast<uint32_t>(std::numeric_limits<GLsizei>::max())
          )
    );
    indexBuffer->release();
  }

  if (shader) {
    if (shader->HasTextureSet()) {
      const auto textureSetRef = shader->TextureSetRef();
      const auto textureSet    = nifFile->GetHeader().GetBlock(textureSetRef);

      for (std::size_t i = 0; i < textureSet->textures.size(); i++) {
        if (auto texturePath = textureSet->textures[i].get(); !texturePath.empty()) {
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
      f->glEnableVertexAttribArray(i);
    } else {
      f->glDisableVertexAttribArray(i);
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
      mat[0], mat[1], mat[2], mat[3], mat[4], mat[5], mat[6], mat[7],
      mat[8], mat[9], mat[10], mat[11], mat[12], mat[13], mat[14], mat[15],
  };
}
