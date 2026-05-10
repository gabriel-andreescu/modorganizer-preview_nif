#include "OpenGLShapeMaterial.h"
#include "NifShaderFlags.h"

#include <QOpenGLShaderProgram>

#include <NifFile.hpp>

#include <algorithm>

namespace {
bool usesEffectShader(const ShaderManager::ShaderType shaderType) {
    return shaderType == ShaderManager::SKEffectShader || shaderType == ShaderManager::FO4EffectShader;
}

QVector2D convertVector2(const nifly::Vector2 vector) {
    return {vector.u, vector.v};
}

QVector3D convertVector3(const nifly::Vector3 vector) {
    return {vector.x, vector.y, vector.z};
}

QColor convertColor(const nifly::Color4 color) {
    return QColor::fromRgbF(color.r, color.g, color.b, color.a);
}

QVector3D colorRgb(const QColor& color) {
    return {color.redF(), color.greenF(), color.blueF()};
}

QVector4D colorRgba(const QColor& color) {
    return {color.redF(), color.greenF(), color.blueF(), color.alphaF()};
}
} // namespace

void OpenGLShapeMaterial::apply(nifly::NiShader* shader, const bool isPBR) {
    m_IsPBR = isPBR;
    applyCommonShaderMaterial(shader);

    if (auto* const bslsp = dynamic_cast<nifly::BSLightingShaderProperty*>(shader)) {
        applyLightingShaderMaterial(bslsp);
    }

    if (auto* const effectShader = dynamic_cast<nifly::BSEffectShaderProperty*>(shader)) {
        applyEffectShaderMaterial(effectShader);
    }
}

void OpenGLShapeMaterial::setupUniforms(
    QOpenGLShaderProgram* program,
    const ShaderManager::ShaderType shaderType
) const {
    program->setUniformValue("ambientColor", QVector4D(0.2f, 0.2f, 0.2f, 1.0f));
    program->setUniformValue("diffuseColor", QVector4D(1.0f, 1.0f, 1.0f, 1.0f));

    program->setUniformValue("alpha", m_Alpha);
    program->setUniformValue("tintColor", m_TintColor);
    program->setUniformValue("uvScale", m_UvScale);
    program->setUniformValue("uvOffset", m_UvOffset);
    program->setUniformValue("specColor", m_SpecColor);
    program->setUniformValue("specStrength", m_SpecStrength);
    program->setUniformValue("specGlossiness", m_SpecGlossiness);
    program->setUniformValue("fresnelPower", m_FresnelPower);

    setupGlowUniforms(program, shaderType);

    program->setUniformValue("hasEmit", m_HasEmit);
    program->setUniformValue("hasSoftlight", m_HasSoftlight);
    program->setUniformValue("hasBacklight", m_HasBacklight);
    program->setUniformValue("hasRimlight", m_HasRimlight);
    program->setUniformValue("hasTintColor", m_HasTintColor);
    program->setUniformValue("hasWeaponBlood", m_HasWeaponBlood);
    program->setUniformValue("greyscaleAlpha", m_GreyscaleAlpha);
    program->setUniformValue("greyscaleColor", m_GreyscaleColor);
    program->setUniformValue("useFalloff", m_UseFalloff);
    program->setUniformValue("falloffParams", m_FalloffParams);
    program->setUniformValue("falloffDepth", m_FalloffDepth);

    program->setUniformValue("softlight", m_Softlight);
    program->setUniformValue("backlightPower", m_BacklightPower);
    program->setUniformValue("rimPower", m_RimPower);
    program->setUniformValue("subsurfaceRolloff", m_SubsurfaceRolloff);

    program->setUniformValue("envReflection", m_EnvReflection);

    setupPBRUniforms(program);
    setupMultilayerUniforms(program, shaderType);
}

void OpenGLShapeMaterial::applyCommonShaderMaterial(nifly::NiShader* shader) {
    m_SpecColor = convertVector3(shader->GetSpecularColor());
    m_SpecStrength = shader->GetSpecularStrength();
    m_SpecGlossiness = qBound(0.0f, shader->GetGlossiness(), 128.0f);
    m_FresnelPower = shader->GetFresnelPower();
    m_PaletteScale = shader->GetGrayscaleToPaletteScale();

    m_HasGlowMap = shader->HasGlowmap();
    m_GlowColor = convertColor(shader->GetEmissiveColor());
    m_GlowMult = shader->GetEmissiveMultiple();

    m_Alpha = shader->GetAlpha();
    m_UvScale = convertVector2(shader->GetUVScale());
    m_UvOffset = convertVector2(shader->GetUVOffset());

    m_HasEmit = shader->IsEmissive();
    m_HasSoftlight = shader->HasSoftlight();
    m_HasBacklight = shader->HasBacklight();
    m_HasRimlight = shader->HasRimlight();

    m_Softlight = shader->GetSoftlight();
    m_BacklightPower = shader->GetBacklightPower();
    m_RimPower = shader->GetRimlightPower();
    m_SubsurfaceRolloff = shader->GetSubsurfaceRolloff();
    m_EnvReflection = shader->GetEnvironmentMapScale();
}

void OpenGLShapeMaterial::applyLightingShaderMaterial(nifly::BSLightingShaderProperty* shader) {
    m_HasRefraction = shader->shaderFlags1 & (SLSF1::Refraction | SLSF1::FireRefraction);
    m_RefractionStrength = shader->refractionStrength;
    const auto bslspType = shader->GetShaderType();
    if (bslspType == nifly::BSLSP_SKINTINT || bslspType == nifly::BSLSP_FACE) {
        m_TintColor = convertVector3(shader->skinTintColor);
        m_HasTintColor = true;
    } else if (bslspType == nifly::BSLSP_HAIRTINT) {
        m_TintColor = convertVector3(shader->hairTintColor);
        m_HasTintColor = true;
    }

    if (bslspType == nifly::BSLSP_MULTILAYERPARALLAX) {
        m_InnerScale = convertVector2(shader->parallaxInnerLayerTextureScale);
        m_InnerThickness = shader->parallaxInnerLayerThickness;
        m_OuterRefraction = shader->parallaxRefractionScale;
        m_OuterReflection = shader->parallaxEnvmapStrength;
    }

    m_HasHeightMap = (bslspType == nifly::BSLSP_PARALLAX || bslspType == nifly::BSLSP_PARALLAXOCC)
                     && (shader->shaderFlags1 & SLSF1::Parallax);

    if (m_IsPBR) {
        configurePBRFlags(shader);

        constexpr float MaxGlintDensity = 40.0f;
        m_PbrParams1 = {
            std::max(shader->GetSpecularStrength(), 0.0f),
            std::max(shader->GetRimlightPower(), 0.0f),
            std::max(shader->GetGlossiness(), 0.0f),
        };
        m_PbrParams2 = QVector4D(m_SpecColor, std::max(shader->GetSubsurfaceRolloff(), 0.0f));

        if (shader->GetShaderType() == nifly::BSLSP_MULTILAYERPARALLAX) {
            const QVector4D rawFeatureParams(
                shader->parallaxInnerLayerThickness,
                shader->parallaxRefractionScale,
                shader->parallaxInnerLayerTextureScale.u,
                shader->parallaxInnerLayerTextureScale.v
            );
            m_PbrFeatureParams = m_PbrHasGlint ? QVector4D(
                                                     rawFeatureParams.x(),
                                                     MaxGlintDensity - rawFeatureParams.y(),
                                                     rawFeatureParams.z(),
                                                     rawFeatureParams.w()
                                                 )
                                               : rawFeatureParams;
        } else if (m_PbrHasTwoLayer) {
            m_PbrFeatureParams = {1.0f, 0.04f, 0.0f, 0.0f};
        } else if (m_PbrHasGlint) {
            m_PbrFeatureParams = {1.5f, 0.0f, 0.015f, 2.0f};
        }
    }
}

void OpenGLShapeMaterial::applyEffectShaderMaterial(nifly::BSEffectShaderProperty* shader) {
    m_HasWeaponBlood = shader->shaderFlags2 & SLSF2::WeaponBlood;
    m_GreyscaleAlpha = shader->shaderFlags1 & SLSF1::GreyscaleToPaletteAlpha;
    m_GreyscaleColor = shader->shaderFlags1 & SLSF1::GreyscaleToPaletteColor;
    m_UseFalloff = shader->shaderFlags1 & SLSF1::UseFalloff;
    m_FalloffParams = QVector4D(
        shader->falloffStartAngle,
        shader->falloffStopAngle,
        shader->falloffStartOpacity,
        shader->falloffStopOpacity
    );
    m_FalloffDepth = shader->softFalloffDepth;
}

void OpenGLShapeMaterial::configurePBRFlags(const nifly::BSLightingShaderProperty* shader) {
    const auto flags2 = shader->shaderFlags2;

    m_PbrHasTwoLayer = HasFlag(flags2, SLSF2::MultiLayerParallax);
    if (m_PbrHasTwoLayer) {
        m_PbrHasInterlayerParallax = HasFlag(flags2, SLSF2::SoftLighting);
        m_PbrHasCoatNormal = HasFlag(flags2, SLSF2::BackLighting);
        m_PbrHasColoredCoat = HasFlag(flags2, SLSF2::EffectLighting);
    } else if (HasFlag(flags2, SLSF2::BackLighting)) {
        m_PbrHasHairMarschner = true;
    } else {
        m_PbrHasSubsurface = HasFlag(flags2, SLSF2::RimLighting);
        m_PbrHasFuzz = HasFlag(flags2, SLSF2::SoftLighting);
        m_PbrHasGlint = !m_PbrHasFuzz && HasFlag(flags2, SLSF2::FitSlope);
    }
}

void OpenGLShapeMaterial::setupGlowUniforms(
    QOpenGLShaderProgram* program,
    const ShaderManager::ShaderType shaderType
) const {
    program->setUniformValue("paletteScale", m_PaletteScale);
    if (usesEffectShader(shaderType)) {
        program->setUniformValue("glowColor", colorRgba(m_GlowColor));
    } else {
        program->setUniformValue("glowColor", colorRgb(m_GlowColor));
    }
    program->setUniformValue("glowMult", m_GlowMult);
}

void OpenGLShapeMaterial::setupPBRUniforms(QOpenGLShaderProgram* program) const {
    if (!m_IsPBR) {
        return;
    }

    program->setUniformValue("pbrHasSubsurface", m_PbrHasSubsurface);
    program->setUniformValue("pbrHasTwoLayer", m_PbrHasTwoLayer);
    program->setUniformValue("pbrHasColoredCoat", m_PbrHasColoredCoat);
    program->setUniformValue("pbrHasInterlayerParallax", m_PbrHasInterlayerParallax);
    program->setUniformValue("pbrHasCoatNormal", m_PbrHasCoatNormal);
    program->setUniformValue("pbrHasFuzz", m_PbrHasFuzz);
    program->setUniformValue("pbrHasHairMarschner", m_PbrHasHairMarschner);
    program->setUniformValue("pbrHasGlint", m_PbrHasGlint);
    program->setUniformValue("pbrParams1", m_PbrParams1);
    program->setUniformValue("pbrParams2", m_PbrParams2);
    program->setUniformValue("pbrFeatureParams", m_PbrFeatureParams);
}

void OpenGLShapeMaterial::setupMultilayerUniforms(
    QOpenGLShaderProgram* program,
    const ShaderManager::ShaderType shaderType
) const {
    if (shaderType != ShaderManager::SKMultilayer) {
        return;
    }

    program->setUniformValue("innerScale", m_InnerScale);
    program->setUniformValue("innerThickness", m_InnerThickness);
    program->setUniformValue("outerRefraction", m_OuterRefraction);
    program->setUniformValue("outerReflection", m_OuterReflection);
}
