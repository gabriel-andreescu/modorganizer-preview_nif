#pragma once

#include "ShaderManager.h"

#include <QColor>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>

class QOpenGLShaderProgram;

namespace nifly {
class BSEffectShaderProperty;
class BSLightingShaderProperty;
class NiShader;
}

struct OpenGLShapeTextureFeatureFlags {
    bool hasGlowMap = false;
    bool hasHeightMap = false;
};

class OpenGLShapeMaterial {
public:
    void apply(nifly::NiShader* shader, bool isPBR);
    void setupUniforms(QOpenGLShaderProgram* program, ShaderManager::ShaderType shaderType) const;

    [[nodiscard]] OpenGLShapeTextureFeatureFlags textureFeatureFlags() const noexcept {
        return {.hasGlowMap = m_HasGlowMap, .hasHeightMap = m_HasHeightMap};
    }
    [[nodiscard]] float alpha() const noexcept {
        return m_Alpha;
    }
    [[nodiscard]] bool hasRefraction() const noexcept {
        return m_HasRefraction;
    }
    [[nodiscard]] float refractionStrength() const noexcept {
        return m_RefractionStrength;
    }

private:
    void applyCommonShaderMaterial(nifly::NiShader* shader);
    void applyLightingShaderMaterial(nifly::BSLightingShaderProperty* shader);
    void applyEffectShaderMaterial(nifly::BSEffectShaderProperty* shader);
    void configurePBRFlags(const nifly::BSLightingShaderProperty* shader);
    void setupGlowUniforms(QOpenGLShaderProgram* program, ShaderManager::ShaderType shaderType) const;
    void setupPBRUniforms(QOpenGLShaderProgram* program) const;
    void setupMultilayerUniforms(QOpenGLShaderProgram* program, ShaderManager::ShaderType shaderType) const;

    bool m_IsPBR = false;

    QVector3D m_SpecColor {1.0f, 1.0f, 1.0f};
    float m_SpecStrength = 1.0f;
    float m_SpecGlossiness = 1.0f;
    float m_FresnelPower = 0.0f;

    float m_PaletteScale = 0.0f;

    bool m_HasGlowMap = false;
    bool m_HasHeightMap = false;
    QColor m_GlowColor = QColorConstants::White;
    float m_GlowMult = 1.0f;

    float m_Alpha = 1.0f;
    QVector3D m_TintColor {1.0f, 1.0f, 1.0f};

    QVector2D m_UvScale {1.0f, 1.0f};
    QVector2D m_UvOffset {0.0f, 0.0f};

    bool m_HasEmit = false;
    bool m_HasSoftlight = false;
    bool m_HasBacklight = false;
    bool m_HasRimlight = false;
    bool m_HasTintColor = false;
    bool m_HasWeaponBlood = false;
    bool m_HasRefraction = false;
    bool m_GreyscaleAlpha = false;
    bool m_GreyscaleColor = false;
    bool m_UseFalloff = false;

    float m_Softlight = 0.3f;
    float m_BacklightPower = 0.0f;
    float m_RimPower = 2.0f;
    float m_SubsurfaceRolloff = 0.0f;
    float m_EnvReflection = 1.0f;
    QVector4D m_FalloffParams {1.0f, 1.0f, 0.0f, 0.0f};
    float m_FalloffDepth = 0.0f;
    float m_RefractionStrength = 0.0f;

    QVector2D m_InnerScale;
    float m_InnerThickness = 0.0f;
    float m_OuterRefraction = 0.0f;
    float m_OuterReflection = 0.0f;

    bool m_PbrHasSubsurface = false;
    bool m_PbrHasTwoLayer = false;
    bool m_PbrHasColoredCoat = false;
    bool m_PbrHasInterlayerParallax = false;
    bool m_PbrHasCoatNormal = false;
    bool m_PbrHasFuzz = false;
    bool m_PbrHasHairMarschner = false;
    bool m_PbrHasGlint = false;
    QVector3D m_PbrParams1 {1.0f, 1.0f, 0.04f};
    QVector4D m_PbrParams2 {1.0f, 1.0f, 1.0f, 0.0f};
    QVector4D m_PbrFeatureParams {0.0f, 0.0f, 0.0f, 0.0f};
};
