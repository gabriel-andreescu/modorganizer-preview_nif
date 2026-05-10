/*
 * PBR material and lighting behavior references Community Shaders' True PBR
 * implementation:
 * https://github.com/community-shaders/skyrim-community-shaders
 *
 * Community Shaders package shaders are licensed under MIT:
 * https://github.com/community-shaders/skyrim-community-shaders/blob/dev/package/Shaders/LICENSE
 *
 * The broader Community Shaders project is GPL-3.0-or-later with modding and
 * linking exceptions:
 * https://github.com/community-shaders/skyrim-community-shaders/blob/dev/COPYING
 * https://github.com/community-shaders/skyrim-community-shaders/blob/dev/EXCEPTIONS.md
 */
#version 120

uniform sampler2D BaseMap;
uniform sampler2D NormalMap;
uniform sampler2D PBREmissiveMap;
uniform sampler2D PBRDisplacementMap;
uniform sampler2D PBRRMAOSMap;
uniform sampler2D PBRFeaturesTexture0;
uniform sampler2D PBRFeaturesTexture1;

uniform vec3 glowColor;
uniform float glowMult;

uniform float alpha;
uniform float alphaThreshold;
uniform int alphaTestMode;

uniform vec3 tintColor;
uniform bool hasTintColor;

uniform vec2 uvScale;
uniform vec2 uvOffset;

uniform bool pbrHasEmissive;
uniform bool pbrHasDisplacement;
uniform bool pbrHasFeaturesTexture0;
uniform bool pbrHasFeaturesTexture1;
uniform bool pbrHasSubsurface;
uniform bool pbrHasTwoLayer;
uniform bool pbrHasColoredCoat;
uniform bool pbrHasInterlayerParallax;
uniform bool pbrHasCoatNormal;
uniform bool pbrHasFuzz;
uniform bool pbrHasHairMarschner;
uniform bool pbrHasGlint;
uniform vec3 pbrParams1;       // roughness scale, displacement scale, specular level
uniform vec4 pbrParams2;       // subsurface/coat color, opacity/strength
uniform vec4 pbrFeatureParams; // coat, fuzz, or glint extension params

varying vec2 TexCoord;
varying vec3 LightDir;
varying vec3 ViewDir;

varying vec4 A;
varying vec4 C;
varying vec4 D;

const float PI = 3.14159265358979323846;
const float INV_PI = 0.3183098861837907;
const float MIN_ROUGHNESS = 0.04;
const float MAX_ROUGHNESS = 1.0;
const float MAX_GLINT_DENSITY = 40.0;
const float EPSILON_DIVISION = 0.000001;
const float EPSILON_DOT_CLAMP = 0.00001;
const float PBR_LIGHTING_COMPENSATION = PI;

// Match Community Shaders' non-linear True PBR lighting scale.
const float PBR_LIGHTING_SCALE = 0.65;

float saturate(float value)
{
    return clamp(value, 0.0, 1.0);
}

vec3 saturate(vec3 value)
{
    return clamp(value, vec3(0.0), vec3(1.0));
}

vec3 tonemap(vec3 x)
{
    float a = 0.15;
    float b = 0.50;
    float c = 0.10;
    float d = 0.20;
    float e = 0.02;
    float f = 0.30;

    return ((x * (a * x + c * b) + d * e) / (x * (a * x + b) + d * f)) - e / f;
}

vec3 srgbToLinear(vec3 color)
{
    return pow(max(color, vec3(0.0)), vec3(2.2));
}

vec3 linearToSrgb(vec3 color)
{
    return pow(max(color, vec3(0.0)), vec3(1.0 / 2.2));
}

vec3 pbrDiffuseColor(vec3 color)
{
    return linearToSrgb(color);
}

bool passesAlphaTest(float value)
{
    if (alphaTestMode == 512) {
        return false;
    }
    if (alphaTestMode == 513) {
        return value < alphaThreshold;
    }
    if (alphaTestMode == 514) {
        return value == alphaThreshold;
    }
    if (alphaTestMode == 515) {
        return value <= alphaThreshold;
    }
    if (alphaTestMode == 516) {
        return value > alphaThreshold;
    }
    if (alphaTestMode == 517) {
        return value != alphaThreshold;
    }
    if (alphaTestMode == 518) {
        return value >= alphaThreshold;
    }

    return true;
}

vec3 sampleNormal(sampler2D normalMap, vec2 uv)
{
    vec3 normal = texture2D(normalMap, uv).xyz * 2.0 - 1.0;
    return normalize(normal);
}

vec2 applyDisplacement(vec2 uv, vec3 viewDir)
{
    if (!pbrHasDisplacement) {
        return uv;
    }

    float height = texture2D(PBRDisplacementMap, uv).r;
    float heightScale = clamp(pbrParams1.y, 0.0, 2.0) * 0.04;
    float viewZ = max(abs(viewDir.z), 0.2);
    return uv + (viewDir.xy / viewZ) * ((height - 0.5) * heightScale);
}

vec3 fresnelSchlick(vec3 f0, float vDotH)
{
    float fc = pow(1.0 - saturate(vDotH), 5.0);
    return vec3(fc) + (vec3(1.0) - vec3(fc)) * f0;
}

float distributionGGX(float roughness, float nDotH)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float nDotH2 = nDotH * nDotH;
    float d = nDotH2 * (a2 - 1.0) + 1.0;
    return a2 / max(PI * d * d, EPSILON_DIVISION);
}

float visibilitySmithJointApprox(float roughness, float nDotV, float nDotL)
{
    float a = roughness * roughness;
    float visV = nDotL * (nDotV * (1.0 + a) + a);
    float visL = nDotV * (nDotL * (1.0 + a) + a);
    return 0.5 / max(visV + visL, EPSILON_DIVISION);
}

vec3 specularMicrofacet(float roughness, vec3 f0, float nDotL, float nDotV,
                        float nDotH, float vDotH, out vec3 fresnel)
{
    float d = distributionGGX(roughness, nDotH);
    float g = visibilitySmithJointApprox(roughness, nDotV, nDotL);
    fresnel = fresnelSchlick(f0, vDotH);
    return d * g * fresnel;
}

float distributionCharlie(float roughness, float nDotH)
{
    float r = max(roughness, MIN_ROUGHNESS);
    float invAlpha = pow(r, -4.0);
    float sin2h = max(1.0 - nDotH * nDotH, 0.0);
    return (2.0 + invAlpha) * pow(sin2h, invAlpha * 0.5) / (2.0 * PI);
}

float visibilityNeubelt(float nDotV, float nDotL)
{
    return 1.0 / max(4.0 * (nDotL + nDotV - nDotL * nDotV), EPSILON_DIVISION);
}

vec3 specularMicroflakes(float roughness, vec3 f0, float nDotL, float nDotV,
                         float nDotH, float vDotH)
{
    float d = distributionCharlie(roughness, nDotH);
    float g = visibilityNeubelt(nDotV, nDotL);
    vec3 f = fresnelSchlick(f0, vDotH);
    return d * g * f;
}

vec2 envBRDF(float roughness, float nDotV)
{
    vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
    vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
    vec4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * nDotV)) * r.x + r.y;
    return vec2(-1.04, 1.04) * a004 + r.zw;
}

vec3 multiBounceAO(vec3 baseColor, float ao)
{
    vec3 a = 2.0404 * baseColor - vec3(0.3324);
    vec3 b = -4.7951 * baseColor + vec3(0.6417);
    vec3 c = 2.7552 * baseColor + vec3(0.6903);
    return max(vec3(ao), ((vec3(ao) * a + b) * ao + c) * ao);
}

float specularOcclusion(float nDotV, float alphaValue, float ao)
{
    return saturate(pow(abs(nDotV + ao), alphaValue) - 1.0 + ao);
}

float hash12(vec2 value)
{
    vec3 p3 = fract(vec3(value.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float hairIOR()
{
    const float n = 1.55;
    const float a = 1.0;
    float ior1 = 2.0 * (n - 1.0) * (a * a) - n + 2.0;
    float ior2 = 2.0 * (n - 1.0) / (a * a) - n + 2.0;
    return 0.5 * ((ior1 + ior2) + 0.5 * (ior1 - ior2));
}

float iorToF0(float ior)
{
    float f0 = (1.0 - ior) / (1.0 + ior);
    return f0 * f0;
}

float hairGaussian(float b, float theta)
{
    float safeB = max(b, EPSILON_DIVISION);
    return exp(-0.5 * theta * theta / (safeB * safeB)) /
           (sqrt(2.0 * PI) * safeB);
}

vec3 hairDiffuseColorMarschner(vec3 normal, vec3 viewDir, vec3 lightDir,
                               float nDotL, float nDotV, float vDotL,
                               float backlit, float area, vec3 baseColor,
                               float roughness)
{
    vec3 scatter = vec3(0.0);

    float cosThetaL = sqrt(max(0.0, 1.0 - nDotL * nDotL));
    float cosThetaV = sqrt(max(0.0, 1.0 - nDotV * nDotV));
    float cosThetaD =
        sqrt(max((1.0 + cosThetaL * cosThetaV + nDotV * nDotL) * 0.5, 0.0));

    vec3 lightProjected = lightDir - nDotL * normal;
    vec3 viewProjected = viewDir - nDotV * normal;
    float cosPhi =
        dot(lightProjected, viewProjected) *
        inversesqrt(max(dot(lightProjected, lightProjected) *
                            dot(viewProjected, viewProjected),
                        EPSILON_DIVISION));
    float cosHalfPhi = sqrt(saturate(0.5 + 0.5 * cosPhi));

    float nPrime = 1.19 / max(cosThetaD, EPSILON_DIVISION) + 0.36 * cosThetaD;
    float shift = 0.0499;
    float hairF0 = iorToF0(hairIOR());
    float thetaH = nDotL + nDotV;

    float mp = hairGaussian(area + roughness, thetaH + shift * 2.0);
    float np = 0.25 * cosHalfPhi;
    float fp = fresnelSchlick(vec3(hairF0),
                              sqrt(saturate(0.5 + 0.5 * vDotL)))
                   .x;
    scatter += (mp * np) * (fp * mix(1.0, backlit, saturate(-vDotL)));

    mp = hairGaussian(area + roughness * 0.5, thetaH - shift);
    float a = (1.55 / hairIOR()) / max(nPrime, EPSILON_DIVISION);
    float h = cosHalfPhi * (1.0 + a * (0.6 - 0.8 * cosPhi));
    fp = fresnelSchlick(vec3(hairF0),
                        cosThetaD * sqrt(saturate(1.0 - h * h)))
             .x;
    float fTransmission = (1.0 - fp) * (1.0 - fp);
    vec3 tp = pow(abs(baseColor),
                  vec3(0.5 * sqrt(max(1.0 - (h * a) * (h * a), 0.0)) /
                       max(cosThetaD, EPSILON_DIVISION)));
    np = exp(-3.65 * cosPhi - 3.98);
    scatter += (mp * np) * (fTransmission * tp) * backlit;

    mp = hairGaussian(area + roughness * 2.0, thetaH - shift * 4.0);
    fp = fresnelSchlick(vec3(hairF0), cosThetaD * 0.5).x;
    fTransmission = (1.0 - fp) * (1.0 - fp) * fp;
    tp = pow(abs(baseColor), vec3(0.8 / max(cosThetaD, EPSILON_DIVISION)));
    np = exp(17.0 * cosPhi - 16.78);
    scatter += (mp * np) * (fTransmission * tp);

    return scatter;
}

vec3 hairDiffuseAttenuationKajiyaKay(vec3 normal, vec3 viewDir, vec3 lightDir,
                                     float nDotL, float nDotV, float shadow,
                                     vec3 baseColor)
{
    float diffuseKajiya = 1.0 - abs(nDotL);
    vec3 fakeNormal = normalize(viewDir - normal * nDotV);
    float wrappedNDotL =
        saturate((dot(fakeNormal, lightDir) + 1.0) / 4.0);
    float diffuseScatter = INV_PI * mix(wrappedNDotL, diffuseKajiya, 0.33);
    float luma = dot(baseColor, vec3(0.2126, 0.7152, 0.0722));
    vec3 scatterTint =
        pow(max(baseColor / max(luma, 0.00001), vec3(0.0)), vec3(1.0 - shadow));

    return sqrt(max(baseColor, vec3(0.0))) * diffuseScatter * scatterTint;
}

vec3 hairColorMarschner(vec3 normal, vec3 viewDir, vec3 lightDir, float nDotL,
                        float nDotV, float vDotL, float shadow, float backlit,
                        float area, vec3 baseColor, float roughness)
{
    return hairDiffuseColorMarschner(normal, viewDir, lightDir, nDotL, nDotV,
                                     vDotL, backlit, area, baseColor,
                                     roughness) +
           hairDiffuseAttenuationKajiyaKay(normal, viewDir, lightDir, nDotL,
                                           nDotV, shadow, baseColor);
}

void main(void)
{
    vec2 originalUv = TexCoord * uvScale + uvOffset;
    vec3 viewDir = normalize(ViewDir);
    vec2 uv = applyDisplacement(originalUv, viewDir);

    vec4 baseMap = texture2D(BaseMap, uv);
    vec3 normal = sampleNormal(NormalMap, uv);

    vec3 lightDir = normalize(LightDir);
    vec3 halfVector = normalize(lightDir + viewDir);

    float rawNDotL = dot(normal, lightDir);
    float rawNDotV = dot(normal, viewDir);
    float nDotL = clamp(rawNDotL, EPSILON_DOT_CLAMP, 1.0);
    float nDotV = saturate(abs(rawNDotV) + EPSILON_DOT_CLAMP);
    float nDotH = saturate(dot(normal, halfVector));
    float vDotH = saturate(dot(viewDir, halfVector));
    float vDotL = dot(viewDir, lightDir);

    vec3 vertexColor = C.rgb;
    vec3 pbrVertexColor = srgbToLinear(vertexColor);
    vec3 baseColorLinear = baseMap.rgb * pbrVertexColor;
    if (hasTintColor) {
        baseColorLinear *= srgbToLinear(tintColor);
    }
    vec3 baseColor = pbrDiffuseColor(baseColorLinear);

    vec4 rawRMAOS =
        texture2D(PBRRMAOSMap, uv) * vec4(pbrParams1.x, 1.0, 1.0, pbrParams1.z);

    float roughness = clamp(rawRMAOS.r, MIN_ROUGHNESS, MAX_ROUGHNESS);
    float metallic = saturate(rawRMAOS.g);
    float ao = saturate(rawRMAOS.b);
    vec3 f0 = mix(vec3(saturate(rawRMAOS.a)), baseColorLinear, metallic);
    vec3 materialBaseColor = baseColor * (1.0 - metallic);
    vec3 directLight = D.rgb * PBR_LIGHTING_COMPENSATION;

    vec3 fresnel;
    vec3 directSpecular =
        specularMicrofacet(roughness, f0, nDotL, nDotV, nDotH, vDotH, fresnel) *
        directLight * nDotL;
    vec3 directDiffuse = directLight * nDotL * INV_PI * (vec3(1.0) - fresnel);
    vec3 transmission = vec3(0.0);
    vec3 coatDiffuse = vec3(0.0);

    if (pbrHasHairMarschner) {
        transmission +=
            directLight * hairColorMarschner(normal, viewDir, lightDir, rawNDotL,
                                             rawNDotV, vDotL, 0.0, 1.0, 0.0,
                                             materialBaseColor, roughness);
    } else {
        if (pbrHasFuzz) {
            vec3 fuzzColor = pbrFeatureParams.rgb;
            float fuzzWeight = saturate(pbrFeatureParams.a);
            if (pbrHasFeaturesTexture1) {
                vec4 sampledFuzz = texture2D(PBRFeaturesTexture1, uv);
                fuzzColor *= pbrDiffuseColor(sampledFuzz.rgb);
                fuzzWeight *= sampledFuzz.a;
            }

            vec3 fuzzSpecular =
                specularMicroflakes(roughness, fuzzColor, nDotL, nDotV, nDotH,
                                    vDotH) *
                directLight * nDotL;
            directSpecular = mix(directSpecular, fuzzSpecular, saturate(fuzzWeight));
        }

        if (pbrHasSubsurface) {
            vec3 subsurfaceColor = pbrParams2.rgb;
            float thickness = saturate(pbrParams2.a);
            if (pbrHasFeaturesTexture0) {
                vec4 sampledSubsurface = texture2D(PBRFeaturesTexture0, uv);
                subsurfaceColor *= pbrDiffuseColor(sampledSubsurface.rgb);
                subsurfaceColor =
                    pbrDiffuseColor(srgbToLinear(subsurfaceColor) * pbrVertexColor);
                thickness *= sampledSubsurface.a;
            }

            float subsurfacePower = 12.234;
            float forwardScatter =
                exp2(saturate(-vDotL) * subsurfacePower - subsurfacePower);
            float backScatter =
                saturate(nDotL * thickness + (1.0 - thickness)) * 0.5;
            float subsurface =
                mix(backScatter, 1.0, forwardScatter) * (1.0 - thickness);
            transmission += subsurfaceColor * subsurface * D.rgb * INV_PI *
                            PBR_LIGHTING_COMPENSATION * (vec3(1.0) - fresnel);
        } else if (pbrHasTwoLayer) {
            vec4 coatColor = pbrParams2;
            if (pbrHasFeaturesTexture0) {
                vec4 sampledCoat = texture2D(PBRFeaturesTexture0, originalUv);
                coatColor.rgb *= pbrDiffuseColor(sampledCoat.rgb);
                coatColor.a *= sampledCoat.a;
            }

            float coatStrength = saturate(coatColor.a);
            float coatRoughness =
                clamp(pbrFeatureParams.x, MIN_ROUGHNESS, MAX_ROUGHNESS);
            vec3 coatF0 = vec3(saturate(pbrFeatureParams.y));

            vec3 coatNormal = normal;
            vec2 coatUv = pbrHasInterlayerParallax ? originalUv : uv;
            if (pbrHasFeaturesTexture1) {
                vec4 sampledCoatProperties = texture2D(PBRFeaturesTexture1, coatUv);
                coatRoughness =
                    clamp(coatRoughness * sampledCoatProperties.a, MIN_ROUGHNESS,
                          MAX_ROUGHNESS);
                if (pbrHasCoatNormal) {
                    coatNormal = normalize(sampledCoatProperties.rgb * 2.0 - 1.0);
                }
            }

            float coatNDotL =
                clamp(dot(coatNormal, lightDir), EPSILON_DOT_CLAMP, 1.0);
            float coatNDotV =
                saturate(abs(dot(coatNormal, viewDir)) + EPSILON_DOT_CLAMP);
            float coatNDotH = saturate(dot(coatNormal, halfVector));
            float coatVDotH = vDotH;

            vec3 coatFresnel;
            vec3 coatSpecular =
                specularMicrofacet(coatRoughness, coatF0, coatNDotL, coatNDotV,
                                   coatNDotH, coatVDotH, coatFresnel) *
                directLight * coatNDotL * coatStrength;

            vec3 layerAttenuation = vec3(1.0) - coatFresnel * coatStrength;
            directDiffuse *= layerAttenuation;
            directSpecular *= layerAttenuation;
            directSpecular += coatSpecular;

            if (pbrHasColoredCoat) {
                coatDiffuse =
                    coatColor.rgb * directLight * coatNDotL * INV_PI *
                    coatStrength;
            }
        }
    }

    if (pbrHasGlint) {
        float screenScale = max(pbrFeatureParams.x, 1.0);
        float density = clamp(MAX_GLINT_DENSITY - pbrFeatureParams.y, 1.0,
                              MAX_GLINT_DENSITY);
        float microfacetRoughness = clamp(pbrFeatureParams.z, 0.005, 0.3);
        float densityRandomization = clamp(pbrFeatureParams.w, 0.0, 5.0);
        float sparkle = smoothstep(1.0 - 1.0 / density, 1.0,
                                   hash12(floor(uv * 1024.0 * screenScale) +
                                          densityRandomization));
        float exponent = mix(512.0, 32.0, microfacetRoughness / 0.3);
        float glint = sparkle * pow(nDotH, exponent) * nDotL;
        directSpecular += vec3(glint) * max(f0, vec3(0.04)) * D.rgb;
    }

    vec2 brdf = envBRDF(roughness, nDotV);
    vec3 indirectDiffuse =
        materialBaseColor * A.rgb * multiBounceAO(materialBaseColor, ao);
    vec3 indirectSpecular =
        (f0 * brdf.x + vec3(brdf.y)) * A.rgb *
        specularOcclusion(nDotV, roughness * roughness, ao);

    if (pbrHasTwoLayer) {
        float coatStrength = saturate(pbrParams2.a);
        vec2 coatBrdf = envBRDF(clamp(pbrFeatureParams.x, MIN_ROUGHNESS,
                                      MAX_ROUGHNESS),
                                nDotV);
        vec3 coatSpecularLobe =
            vec3(saturate(pbrFeatureParams.y)) * coatBrdf.x + vec3(coatBrdf.y);
        vec3 layerAttenuation = vec3(1.0) - coatSpecularLobe * coatStrength;
        indirectDiffuse *= layerAttenuation;
        indirectSpecular *= layerAttenuation;
        indirectSpecular += coatSpecularLobe * A.rgb * coatStrength;
    }

    vec3 colorLinear = directDiffuse * materialBaseColor + directSpecular +
                       coatDiffuse + indirectDiffuse + indirectSpecular +
                       transmission;

    if (pbrHasEmissive) {
        vec3 emissive = glowColor * max(glowMult, 0.0) *
                        texture2D(PBREmissiveMap, uv).rgb * vertexColor;
        colorLinear += emissive;
    }

    colorLinear *= PBR_LIGHTING_SCALE;

    vec4 color;
    color.rgb = tonemap(colorLinear) / tonemap(vec3(1.0));
    color.a = C.a * baseMap.a;

    if (!passesAlphaTest(color.a)) {
        discard;
    }

    color.a *= alpha;
    gl_FragColor = color;
}
