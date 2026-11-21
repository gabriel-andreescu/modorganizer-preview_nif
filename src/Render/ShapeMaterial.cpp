#include "Render/ShapeMaterial.h"
#include "Animation/ShaderValues.h"
#include "Nif/ShaderFlags.h"

#include <QOpenGLShaderProgram>

#include <nifly/NifFile.hpp>

#include <algorithm>
#include <cmath>

namespace {
bool UsesEffectShader(const ShaderManager::ShaderType a_shaderType) {
    return a_shaderType == ShaderManager::kSKEffectShader || a_shaderType == ShaderManager::kFO4EffectShader;
}

QVector2D ConvertVector2(const nifly::Vector2 a_vector) {
    return {a_vector.u, a_vector.v};
}

QVector3D ConvertVector3(const nifly::Vector3 a_vector) {
    return {a_vector.x, a_vector.y, a_vector.z};
}

QColor ConvertColor(const nifly::Color4 a_color) {
    return QColor::fromRgbF(a_color.r, a_color.g, a_color.b, a_color.a);
}

QVector3D ColorRgb(const QColor& a_color) {
    return {a_color.redF(), a_color.greenF(), a_color.blueF()};
}

QVector4D ColorRgba(const QColor& a_color) {
    return {a_color.redF(), a_color.greenF(), a_color.blueF(), a_color.alphaF()};
}
} // namespace

void OpenGLShapeMaterial::Apply(
    nifly::NiShader* a_shader,
    const bool a_isPBR,
    const Fo4Material::Material& a_material
) {
    _isPBR = a_isPBR;
    ApplyCommonShaderMaterial(a_shader);
    ApplyPalette(a_shader, a_material);

    if (auto* const bslsp = dynamic_cast<nifly::BSLightingShaderProperty*>(a_shader)) {
        ApplyLightingShaderMaterial(bslsp);
    }

    if (auto* const effectShader = dynamic_cast<nifly::BSEffectShaderProperty*>(a_shader)) {
        _isEffectShader = true;
        ApplyEffectShaderMaterial(effectShader);
    }
}

OpenGLShapeMaterial OpenGLShapeMaterial::Animated(const Animation::ShaderValues& a_values) const {
    using Animation::ShaderColor;
    using Animation::ShaderFloat;

    auto material = *this;
    material.AnimateSurface(a_values);
    material.AnimateFalloff(a_values);
    material.AnimateSpecular(a_values);

    if (const auto& color = a_values[ShaderColor::kEmissive]) {
        material._glowColor.setRgbF(color->x, color->y, color->z, material._glowColor.alphaF());
    }
    if (const auto& alpha = a_values[ShaderFloat::kAlpha]) {
        // An effect shader's opacity is the alpha of its base color.
        if (_isEffectShader) {
            material._glowColor.setAlphaF(qBound(0.0F, *alpha, 1.0F));
        } else {
            material._alpha = *alpha;
        }
    }
    return material;
}

void OpenGLShapeMaterial::AnimateSurface(const Animation::ShaderValues& a_values) {
    using Animation::ShaderFloat;

    _glowMult = a_values[ShaderFloat::kEmissiveMultiple].value_or(_glowMult);
    _uvOffset = {
        a_values[ShaderFloat::kUOffset].value_or(_uvOffset.x()),
        a_values[ShaderFloat::kVOffset].value_or(_uvOffset.y()),
    };
    _uvScale = {
        a_values[ShaderFloat::kUScale].value_or(_uvScale.x()),
        a_values[ShaderFloat::kVScale].value_or(_uvScale.y()),
    };
    _refractionStrength = a_values[ShaderFloat::kRefractionStrength].value_or(_refractionStrength);
    _envReflection = a_values[ShaderFloat::kReflectionStrength].value_or(_envReflection);
}

void OpenGLShapeMaterial::AnimateFalloff(const Animation::ShaderValues& a_values) {
    using Animation::ShaderFloat;

    // Keys give the falloff angles in degrees. The shader block and the shaders hold their cosines, and the games
    // convert the same way.
    if (const auto& degrees = a_values[ShaderFloat::kFalloffStartAngle]) {
        _falloffParams.setX(std::cos(qDegreesToRadians(*degrees)));
    }
    if (const auto& degrees = a_values[ShaderFloat::kFalloffStopAngle]) {
        _falloffParams.setY(std::cos(qDegreesToRadians(*degrees)));
    }
    _falloffParams.setZ(a_values[ShaderFloat::kFalloffStartOpacity].value_or(_falloffParams.z()));
    _falloffParams.setW(a_values[ShaderFloat::kFalloffStopOpacity].value_or(_falloffParams.w()));
}

// The PBR shader reads its specular values from its own parameters, so those follow too.
void OpenGLShapeMaterial::AnimateSpecular(const Animation::ShaderValues& a_values) {
    using Animation::ShaderColor;
    using Animation::ShaderFloat;

    if (const auto& strength = a_values[ShaderFloat::kSpecularStrength]) {
        _specStrength = *strength;
        _pbrParams1.setX(std::max(*strength, 0.0F));
    }
    if (const auto& glossiness = a_values[ShaderFloat::kGlossiness]) {
        _specGlossiness = qBound(0.0F, *glossiness, 128.0F);
        _pbrParams1.setZ(std::max(*glossiness, 0.0F));
    }
    if (const auto& color = a_values[ShaderColor::kSpecular]) {
        _specColor = ConvertVector3(*color);
        _pbrParams2 = QVector4D(_specColor, _pbrParams2.w());
    }
}

void OpenGLShapeMaterial::SetupUniforms(
    QOpenGLShaderProgram* a_program,
    const ShaderManager::ShaderType a_shaderType
) const {
    a_program->setUniformValue("ambientColor", QVector4D(0.2F, 0.2F, 0.2F, 1.0F));
    a_program->setUniformValue("diffuseColor", QVector4D(1.0F, 1.0F, 1.0F, 1.0F));

    a_program->setUniformValue("alpha", _alpha);
    a_program->setUniformValue("tintColor", _tintColor);
    a_program->setUniformValue("uvScale", _uvScale);
    a_program->setUniformValue("uvOffset", _uvOffset);
    a_program->setUniformValue("specColor", _specColor);
    a_program->setUniformValue("specStrength", _specStrength);
    a_program->setUniformValue("specGlossiness", _specGlossiness);
    a_program->setUniformValue("fresnelPower", _fresnelPower);

    SetupGlowUniforms(a_program, a_shaderType);

    a_program->setUniformValue("hasEmit", static_cast<GLint>(_hasEmit));
    a_program->setUniformValue("hasSoftlight", static_cast<GLint>(_hasSoftlight));
    a_program->setUniformValue("hasBacklight", static_cast<GLint>(_hasBacklight));
    a_program->setUniformValue("hasRimlight", static_cast<GLint>(_hasRimlight));
    a_program->setUniformValue("hasTintColor", static_cast<GLint>(_hasTintColor));
    a_program->setUniformValue("hasWeaponBlood", static_cast<GLint>(_hasWeaponBlood));
    a_program->setUniformValue("greyscaleAlpha", static_cast<GLint>(_greyscaleAlpha));
    a_program->setUniformValue("greyscaleColor", static_cast<GLint>(_greyscaleColor));
    a_program->setUniformValue("useFalloff", static_cast<GLint>(_useFalloff));
    a_program->setUniformValue("falloffParams", _falloffParams);
    a_program->setUniformValue("falloffDepth", _falloffDepth);

    a_program->setUniformValue("softlight", _softlight);
    a_program->setUniformValue("backlightPower", _backlightPower);
    a_program->setUniformValue("rimPower", _rimPower);
    a_program->setUniformValue("subsurfaceRolloff", _subsurfaceRolloff);

    a_program->setUniformValue("envReflection", _envReflection);

    SetupPBRUniforms(a_program);
    SetupMultilayerUniforms(a_program, a_shaderType);
}

void OpenGLShapeMaterial::ApplyCommonShaderMaterial(nifly::NiShader* a_shader) {
    _specColor = ConvertVector3(a_shader->GetSpecularColor());
    _specStrength = a_shader->GetSpecularStrength();
    _specGlossiness = qBound(0.0F, a_shader->GetGlossiness(), 128.0F);
    _fresnelPower = a_shader->GetFresnelPower();

    _hasGlowMap = a_shader->HasGlowmap();
    _glowColor = ConvertColor(a_shader->GetEmissiveColor());
    _glowMult = a_shader->GetEmissiveMultiple();

    _alpha = a_shader->GetAlpha();
    _uvScale = ConvertVector2(a_shader->GetUVScale());
    _uvOffset = ConvertVector2(a_shader->GetUVOffset());

    _hasEmit = a_shader->IsEmissive();
    _hasSoftlight = a_shader->HasSoftlight();
    _hasBacklight = a_shader->HasBacklight();
    _hasRimlight = a_shader->HasRimlight();

    _softlight = a_shader->GetSoftlight();
    _backlightPower = a_shader->GetBacklightPower();
    _rimPower = a_shader->GetRimlightPower();
    _subsurfaceRolloff = a_shader->GetSubsurfaceRolloff();
    _envReflection = a_shader->GetEnvironmentMapScale();
}

void OpenGLShapeMaterial::ApplyLightingShaderMaterial(nifly::BSLightingShaderProperty* a_shader) {
    _hasRefraction = ((a_shader->shaderFlags1 & (SLSF1::kRefraction | SLSF1::kFireRefraction)) != 0U);
    _refractionStrength = a_shader->refractionStrength;
    const auto bslspType = a_shader->GetShaderType();
    // Fallout 4's lighting shaders never read the skin or hair tint, and its body NIFs store a black
    // one, so fo4_default.frag applies none.
    if (bslspType == nifly::BSLSP_SKINTINT || bslspType == nifly::BSLSP_FACE) {
        _tintColor = ConvertVector3(a_shader->skinTintColor);
        _hasTintColor = true;
    } else if (bslspType == nifly::BSLSP_HAIRTINT) {
        _tintColor = ConvertVector3(a_shader->hairTintColor);
        _hasTintColor = true;
    }

    if (bslspType == nifly::BSLSP_MULTILAYERPARALLAX) {
        _innerScale = ConvertVector2(a_shader->parallaxInnerLayerTextureScale);
        _innerThickness = a_shader->parallaxInnerLayerThickness;
        _outerRefraction = a_shader->parallaxRefractionScale;
        _outerReflection = a_shader->parallaxEnvmapStrength;
    }

    _hasHeightMap = (bslspType == nifly::BSLSP_PARALLAX || bslspType == nifly::BSLSP_PARALLAXOCC)
                    && ((a_shader->shaderFlags1 & SLSF1::kParallax) != 0U);

    if (_isPBR) {
        ConfigurePBRFlags(a_shader);

        constexpr float kMaxGlintDensity = 40.0F;
        _pbrParams1 = {
            std::max(a_shader->GetSpecularStrength(), 0.0F),
            std::max(a_shader->GetRimlightPower(), 0.0F),
            std::max(a_shader->GetGlossiness(), 0.0F),
        };
        _pbrParams2 = QVector4D(_specColor, std::max(a_shader->GetSubsurfaceRolloff(), 0.0F));

        if (a_shader->GetShaderType() == nifly::BSLSP_MULTILAYERPARALLAX) {
            const QVector4D rawFeatureParams(
                a_shader->parallaxInnerLayerThickness,
                a_shader->parallaxRefractionScale,
                a_shader->parallaxInnerLayerTextureScale.u,
                a_shader->parallaxInnerLayerTextureScale.v
            );
            _pbrFeatureParams = _pbrHasGlint ? QVector4D(
                                                   rawFeatureParams.x(),
                                                   kMaxGlintDensity - rawFeatureParams.y(),
                                                   rawFeatureParams.z(),
                                                   rawFeatureParams.w()
                                               )
                                             : rawFeatureParams;
        } else if (_pbrHasTwoLayer) {
            _pbrFeatureParams = {1.0F, 0.04F, 0.0F, 0.0F};
        } else if (_pbrHasGlint) {
            _pbrFeatureParams = {1.5F, 0.0F, 0.015F, 2.0F};
        }
    }
}

void OpenGLShapeMaterial::ApplyEffectShaderMaterial(nifly::BSEffectShaderProperty* a_shader) {
    _hasWeaponBlood = ((a_shader->shaderFlags2 & SLSF2::kWeaponBlood) != 0U);
    _useFalloff = ((a_shader->shaderFlags1 & SLSF1::kUseFalloff) != 0U);
    _falloffParams = QVector4D(
        a_shader->falloffStartAngle,
        a_shader->falloffStopAngle,
        a_shader->falloffStartOpacity,
        a_shader->falloffStopOpacity
    );
    _falloffDepth = a_shader->softFalloffDepth;
}

void OpenGLShapeMaterial::ApplyPalette(const nifly::NiShader* a_shader, const Fo4Material::Material& a_material) {
    // Fallout 4 takes the palette settings from the material file when the shape has one.
    if (a_material.valid) {
        _greyscaleColor = a_material.palette.color;
        _greyscaleAlpha = a_material.palette.alpha;
        _paletteScale = a_material.palette.scale;
        return;
    }

    if (const auto* const bsShader = dynamic_cast<const nifly::BSShaderProperty*>(a_shader)) {
        _greyscaleColor = ((bsShader->shaderFlags1 & SLSF1::kGreyscaleToPaletteColor) != 0U);
        _greyscaleAlpha = ((bsShader->shaderFlags1 & SLSF1::kGreyscaleToPaletteAlpha) != 0U);
    }
    _paletteScale = a_shader->GetGrayscaleToPaletteScale();
}

void OpenGLShapeMaterial::ConfigurePBRFlags(const nifly::BSLightingShaderProperty* a_shader) {
    const auto flags2 = a_shader->shaderFlags2;

    _pbrHasTwoLayer = HasFlag(flags2, SLSF2::kMultiLayerParallax);
    if (_pbrHasTwoLayer) {
        _pbrHasInterlayerParallax = HasFlag(flags2, SLSF2::kSoftLighting);
        _pbrHasCoatNormal = HasFlag(flags2, SLSF2::kBackLighting);
        _pbrHasColoredCoat = HasFlag(flags2, SLSF2::kEffectLighting);
    } else if (HasFlag(flags2, SLSF2::kBackLighting)) {
        _pbrHasHairMarschner = true;
    } else {
        _pbrHasSubsurface = HasFlag(flags2, SLSF2::kRimLighting);
        _pbrHasFuzz = HasFlag(flags2, SLSF2::kSoftLighting);
        _pbrHasGlint = !_pbrHasFuzz && HasFlag(flags2, SLSF2::kFitSlope);
    }
}

void OpenGLShapeMaterial::SetupGlowUniforms(
    QOpenGLShaderProgram* a_program,
    const ShaderManager::ShaderType a_shaderType
) const {
    a_program->setUniformValue("paletteScale", _paletteScale);
    if (UsesEffectShader(a_shaderType)) {
        a_program->setUniformValue("glowColor", ColorRgba(_glowColor));
    } else {
        a_program->setUniformValue("glowColor", ColorRgb(_glowColor));
    }
    a_program->setUniformValue("glowMult", _glowMult);
}

void OpenGLShapeMaterial::SetupPBRUniforms(QOpenGLShaderProgram* a_program) const {
    if (!_isPBR) {
        return;
    }

    a_program->setUniformValue("pbrHasSubsurface", static_cast<GLint>(_pbrHasSubsurface));
    a_program->setUniformValue("pbrHasTwoLayer", static_cast<GLint>(_pbrHasTwoLayer));
    a_program->setUniformValue("pbrHasColoredCoat", static_cast<GLint>(_pbrHasColoredCoat));
    a_program->setUniformValue("pbrHasInterlayerParallax", static_cast<GLint>(_pbrHasInterlayerParallax));
    a_program->setUniformValue("pbrHasCoatNormal", static_cast<GLint>(_pbrHasCoatNormal));
    a_program->setUniformValue("pbrHasFuzz", static_cast<GLint>(_pbrHasFuzz));
    a_program->setUniformValue("pbrHasHairMarschner", static_cast<GLint>(_pbrHasHairMarschner));
    a_program->setUniformValue("pbrHasGlint", static_cast<GLint>(_pbrHasGlint));
    a_program->setUniformValue("pbrParams1", _pbrParams1);
    a_program->setUniformValue("pbrParams2", _pbrParams2);
    a_program->setUniformValue("pbrFeatureParams", _pbrFeatureParams);
}

void OpenGLShapeMaterial::SetupMultilayerUniforms(
    QOpenGLShaderProgram* a_program,
    const ShaderManager::ShaderType a_shaderType
) const {
    if (a_shaderType != ShaderManager::kSKMultilayer) {
        return;
    }

    a_program->setUniformValue("innerScale", _innerScale);
    a_program->setUniformValue("innerThickness", _innerThickness);
    a_program->setUniformValue("outerRefraction", _outerRefraction);
    a_program->setUniformValue("outerReflection", _outerReflection);
}
