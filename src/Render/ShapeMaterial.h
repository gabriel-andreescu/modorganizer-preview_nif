#pragma once

#include "Nif/Fo4Material.h"
#include "Render/ShaderManager.h"

#include <QColor>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>

class QOpenGLShaderProgram;

namespace Animation {
struct ShaderValues;
}

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
    void Apply(nifly::NiShader* a_shader, bool a_isPBR, const Fo4Material::Material& a_material);
    // A copy with the animated values in place of the shader's own.
    [[nodiscard]] OpenGLShapeMaterial Animated(const Animation::ShaderValues& a_values) const;
    void SetupUniforms(QOpenGLShaderProgram* a_program, ShaderManager::ShaderType a_shaderType) const;

    [[nodiscard]] OpenGLShapeTextureFeatureFlags TextureFeatureFlags() const noexcept {
        return {.hasGlowMap = _hasGlowMap, .hasHeightMap = _hasHeightMap};
    }
    [[nodiscard]] float Alpha() const noexcept {
        return _alpha;
    }
    [[nodiscard]] bool HasRefraction() const noexcept {
        return _hasRefraction;
    }
    [[nodiscard]] float RefractionStrength() const noexcept {
        return _refractionStrength;
    }

private:
    void AnimateSurface(const Animation::ShaderValues& a_values);
    void AnimateFalloff(const Animation::ShaderValues& a_values);
    void AnimateSpecular(const Animation::ShaderValues& a_values);
    void ApplyCommonShaderMaterial(nifly::NiShader* a_shader);
    void ApplyLightingShaderMaterial(nifly::BSLightingShaderProperty* a_shader);
    void ApplyEffectShaderMaterial(nifly::BSEffectShaderProperty* a_shader);
    void ApplyPalette(const nifly::NiShader* a_shader, const Fo4Material::Material& a_material);
    void ConfigurePBRFlags(const nifly::BSLightingShaderProperty* a_shader);
    void SetupGlowUniforms(QOpenGLShaderProgram* a_program, ShaderManager::ShaderType a_shaderType) const;
    void SetupPBRUniforms(QOpenGLShaderProgram* a_program) const;
    void SetupMultilayerUniforms(QOpenGLShaderProgram* a_program, ShaderManager::ShaderType a_shaderType) const;

    bool _isPBR = false;
    bool _isEffectShader = false;

    QVector3D _specColor {1.0F, 1.0F, 1.0F};
    float _specStrength = 1.0F;
    float _specGlossiness = 1.0F;
    float _fresnelPower = 0.0F;

    float _paletteScale = 0.0F;

    bool _hasGlowMap = false;
    bool _hasHeightMap = false;
    QColor _glowColor = QColorConstants::White;
    float _glowMult = 1.0F;

    float _alpha = 1.0F;
    QVector3D _tintColor {1.0F, 1.0F, 1.0F};

    QVector2D _uvScale {1.0F, 1.0F};
    QVector2D _uvOffset {0.0F, 0.0F};

    bool _hasEmit = false;
    bool _hasSoftlight = false;
    bool _hasBacklight = false;
    bool _hasRimlight = false;
    bool _hasTintColor = false;
    bool _hasWeaponBlood = false;
    bool _hasRefraction = false;
    bool _greyscaleAlpha = false;
    bool _greyscaleColor = false;
    bool _useFalloff = false;

    float _softlight = 0.3F;
    float _backlightPower = 0.0F;
    float _rimPower = 2.0F;
    float _subsurfaceRolloff = 0.0F;
    float _envReflection = 1.0F;
    QVector4D _falloffParams {1.0F, 1.0F, 0.0F, 0.0F};
    float _falloffDepth = 0.0F;
    float _refractionStrength = 0.0F;

    QVector2D _innerScale;
    float _innerThickness = 0.0F;
    float _outerRefraction = 0.0F;
    float _outerReflection = 0.0F;

    bool _pbrHasSubsurface = false;
    bool _pbrHasTwoLayer = false;
    bool _pbrHasColoredCoat = false;
    bool _pbrHasInterlayerParallax = false;
    bool _pbrHasCoatNormal = false;
    bool _pbrHasFuzz = false;
    bool _pbrHasHairMarschner = false;
    bool _pbrHasGlint = false;
    QVector3D _pbrParams1 {1.0F, 1.0F, 0.04F};
    QVector4D _pbrParams2 {1.0F, 1.0F, 1.0F, 0.0F};
    QVector4D _pbrFeatureParams {0.0F, 0.0F, 0.0F, 0.0F};
};
