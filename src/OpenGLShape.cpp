#include "OpenGLShape.h"

#include <QOpenGLContext>
#include <QOpenGLFunctions_2_1>

OpenGLShape::OpenGLShape(
    nifly::NifFile* nifFile,
    nifly::NiShape* niShape,
    TextureManager* textureManager)
{
    auto f = QOpenGLContext::currentContext()->versionFunctions<QOpenGLFunctions_2_1>();

    auto shader = nifFile->GetShader(niShape);
    auto& version = nifFile->GetHeader().GetVersion();
    if (version.IsFO4()) {
        if (shader && shader->GetBlockName() == "BSEffectShaderProperty") {
            shaderType = ShaderManager::FO4EffectShader;
        }
        else {
            shaderType = ShaderManager::FO4Default;
        }
    }
    else {
        if (shader && shader->GetBlockName() == "BSEffectShaderProperty") {
            shaderType = ShaderManager::SKEffectShader;
        }
        else {
            if (shader && shader->IsModelSpace()) {
                shaderType = ShaderManager::SKMSN;
            }
            else if (shader && shader->bslspShaderType == nifly::BSLSP_MULTILAYERPARALLAX) {
                shaderType = ShaderManager::SKMultilayer;
            }
            else {
                shaderType = ShaderManager::SKDefault;
            }
        }
    }

    vertexArray = new QOpenGLVertexArrayObject();
    vertexArray->create();
    auto binder = QOpenGLVertexArrayObject::Binder(vertexArray);

    auto numVertices = niShape->GetNumVertices();

    nifly::MatTransform transform;
    nifFile->GetNodeTransformToGlobal(niShape->name.get(), transform);
    modelMatrix = convertTransform(transform);

    if (niShape->HasVertices()) {
        vertexBuffer = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
        if (vertexBuffer->create()) {
            vertexBuffer->bind();

            if (auto vertices = nifFile->GetVertsForShape(niShape)) {
                vertexBuffer->allocate(
                    vertices->data(),
                    vertices->size() * sizeof(nifly::Vector3));
            }

            f->glEnableVertexAttribArray(0);
            f->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(nifly::Vector3), nullptr);
            vertexBuffer->release();
        }
    }

    if (niShape->HasNormals()) {
        normalBuffer = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
        if (normalBuffer->create()) {
            normalBuffer->bind();

            if (auto normals = nifFile->GetNormalsForShape(niShape)) {
                normalBuffer->allocate(
                    normals->data(),
                    normals->size() * sizeof(nifly::Vector3));
            }

            f->glEnableVertexAttribArray(1);
            f->glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(nifly::Vector3), nullptr);
            normalBuffer->release();
        }
    }

    if (niShape->HasTangents()) {
        tangentBuffer = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
        if (tangentBuffer->create()) {
            tangentBuffer->bind();

            if (auto tangents = nifFile->GetTangentsForShape(niShape)) {
                tangentBuffer->allocate(
                    tangents->data(),
                    tangents->size() * sizeof(nifly::Vector3));
            }

            f->glEnableVertexAttribArray(2);
            f->glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(nifly::Vector3), nullptr);
            tangentBuffer->release();
        }

        bitangentBuffer = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
        if (bitangentBuffer->create()) {
            bitangentBuffer->bind();

            if (auto bitangents = nifFile->GetBitangentsForShape(niShape)) {
                bitangentBuffer->allocate(
                    bitangents->data(),
                    bitangents->size() * sizeof(nifly::Vector3));
            }

            f->glEnableVertexAttribArray(3);
            f->glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(nifly::Vector3), nullptr);
            bitangentBuffer->release();
        }
    }

    if (niShape->HasUVs()) {
        texCoordBuffer = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
        if (texCoordBuffer->create()) {
            texCoordBuffer->bind();

            if (auto texCoords = nifFile->GetUvsForShape(niShape)) {
                texCoordBuffer->allocate(
                    texCoords->data(),
                    texCoords->size() * sizeof(nifly::Vector2));
            }

            f->glEnableVertexAttribArray(4);
            f->glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, sizeof(nifly::Vector2), nullptr);
            texCoordBuffer->release();
        }
    }

    if (!niShape->HasVertexColors()) {
        niShape->SetVertexColors(true);
    }

    colorBuffer = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    if (colorBuffer->create()) {
        colorBuffer->bind();

        std::vector<nifly::Color4> colors;
        if (nifFile->GetColorsForShape(niShape, colors)) {
            colorBuffer->allocate(
                colors.data(),
                colors.size() * sizeof(nifly::Color4));
        }

        f->glEnableVertexAttribArray(5);
        f->glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(nifly::Color4), nullptr);
        colorBuffer->release();
    }

    indexBuffer = new QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
    if (indexBuffer->create()) {
        indexBuffer->bind();

        std::vector<nifly::Triangle> tris;
        if (niShape->GetTriangles(tris)) {
            indexBuffer->allocate(tris.data(), tris.size() * sizeof(nifly::Triangle));
        }

        elements = niShape->GetNumTriangles() * 3;
        indexBuffer->release();
    }

    if (shader) {
        if (shader->HasTextureSet()) {
            auto textureSetRef = shader->TextureSetRef();
            auto textureSet = nifFile->GetHeader().GetBlock(textureSetRef);

            for (std::size_t i = 0; i < textureSet->textures.size(); i++) {
                auto texturePath = textureSet->textures[i].get();
                if (!texturePath.empty()) {
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
                        }
                        else {
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

        specColor = convertVector3(shader->GetSpecularColor());
        specStrength = shader->GetSpecularStrength();
        specGlossiness = qBound(0.0f, shader->GetGlossiness(), 128.0f);
        fresnelPower = shader->GetFresnelPower();
        paletteScale = shader->GetGrayscaleToPaletteScale();

        hasGlowMap = shader->HasGlowmap();
        glowColor = convertColor(shader->GetEmissiveColor());
        glowMult = shader->GetEmissiveMultiple();

        alpha = shader->GetAlpha();
        tintColor = { 1.0f, 1.0f, 1.0f };

        uvScale = convertVector2(shader->GetUVScale());
        uvOffset = convertVector2(shader->GetUVOffset());

        hasEmit = shader->IsEmissive();
        hasSoftlight = shader->HasSoftlight();
        hasBacklight = shader->HasBacklight();
        hasRimlight = shader->HasRimlight();
        hasTintColor = shader->IsSkinTinted();

        softlight = shader->GetSoftlight();
        backlightPower = shader->GetBacklightPower();
        rimPower = shader->GetRimlightPower();
        doubleSided = shader->IsDoubleSided();
        envReflection = shader->GetEnvironmentMapScale();

        if (auto alphaProperty = nifFile->GetAlphaProperty(niShape)) {
            alphaThreshold = alphaProperty->threshold / 255.0f;
        }

        if (shader->bslspShaderType == nifly::BSLSP_MULTILAYERPARALLAX) {
            if (auto bslsp = dynamic_cast<nifly::BSLightingShaderProperty*>(shader)) {
                innerScale = convertVector2(bslsp->parallaxInnerLayerTextureScale);
                innerThickness = bslsp->parallaxInnerLayerThickness;
                outerRefraction = bslsp->parallaxRefractionScale;
                outerReflection = bslsp->parallaxEnvmapStrength;
            }
        }
    }
    else {
        textures[BaseMap] = textureManager->getWhiteTexture();
        textures[NormalMap] = textureManager->getFlatNormalTexture();
    }
}

void OpenGLShape::destroy()
{
    if (vertexBuffer) {
        vertexBuffer->destroy();
        delete vertexBuffer;
    }

    if (normalBuffer) {
        normalBuffer->destroy();
        delete normalBuffer;
    }

    if (tangentBuffer) {
        tangentBuffer->destroy();
        delete tangentBuffer;
    }

    if (bitangentBuffer) {
        bitangentBuffer->destroy();
        delete bitangentBuffer;
    }

    if (texCoordBuffer) {
        texCoordBuffer->destroy();
        delete texCoordBuffer;
    }

    if (colorBuffer) {
        colorBuffer->destroy();
        delete colorBuffer;
    }

    if (indexBuffer) {
        indexBuffer->destroy();
        delete indexBuffer;
    }

    if (vertexArray) {
        vertexArray->destroy();
        vertexArray->deleteLater();
    }
}

void OpenGLShape::setupShaders(QOpenGLShaderProgram* program)
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

    auto f = QOpenGLContext::currentContext()->versionFunctions<QOpenGLFunctions_2_1>();

    if (doubleSided) {
        f->glDisable(GL_CULL_FACE);
    }
    else {
        f->glEnable(GL_CULL_FACE);
        f->glCullFace(GL_BACK);
    }

    f->glAlphaFunc(GL_GREATER, alphaThreshold);
}

QVector2D OpenGLShape::convertVector2(nifly::Vector2 vector)
{
    return { vector.u, vector.v };
}

QVector3D OpenGLShape::convertVector3(nifly::Vector3 vector)
{
    return { vector.x, vector.y, vector.z };
}

QColor OpenGLShape::convertColor(nifly::Color4 color)
{
    return QColor::fromRgbF(color.r, color.g, color.b, color.a);
}

QMatrix4x4 OpenGLShape::convertTransform(nifly::MatTransform transform)
{
    auto mat = transform.ToMatrix();
    return QMatrix4x4{
        mat[0],  mat[1],  mat[2],  mat[3],
        mat[4],  mat[5],  mat[6],  mat[7],
        mat[8],  mat[9],  mat[10], mat[11],
        mat[12], mat[13], mat[14], mat[15],
    };
}
