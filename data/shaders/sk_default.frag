#version 120

uniform sampler2D BaseMap;
uniform sampler2D NormalMap;
uniform sampler2D GlowMap;
uniform sampler2D HeightMap;
uniform sampler2D LightMask;
uniform sampler2D BacklightMap;
uniform sampler2D EnvironmentMap;
uniform samplerCube CubeMap;

uniform vec3  specColor;
uniform float specStrength;
uniform float specGlossiness;

uniform bool  hasGlowMap;
uniform vec3  glowColor;
uniform float glowMult;

uniform float alpha;

uniform vec3  tintColor;
uniform bool  hasTintColor;

uniform bool  hasHeightMap;
uniform vec2  uvScale;
uniform vec2  uvOffset;

uniform bool  hasEmit;
uniform bool  hasSoftlight;
uniform bool  hasBacklight;
uniform bool  hasRimlight;
uniform bool  hasCubeMap;
uniform bool  hasEnvMask;

uniform float softlight;
uniform float rimPower;

uniform float envReflection;

uniform mat4 modelViewMatrixInverse;
uniform mat4 worldMatrix;

varying vec2  vTexCoord;
varying vec3  vLightDir;
varying vec3  vViewDir;

varying vec3  vNormal;
varying vec3  vTangent;
varying vec3  vBitangent;
varying vec3  vPosViewSpace;

varying vec4  vAmbientColor;
varying vec4  vVertexColor;
varying vec4  vDiffuseColor;

vec3 tonemap(vec3 x)
{
    float _A = 0.15;
    float _B = 0.50;
    float _C = 0.10;
    float _D = 0.20;
    float _E = 0.02;
    float _F = 0.30;

    return ((x * (_A*x + _C*_B) + _D*_E) /
    (x * (_A*x + _B)       + _D*_F))
    - (_E / _F);
}

void main(void)
{
    vec2 offset = vTexCoord * uvScale + uvOffset;
    vec3 E = normalize(vViewDir);

    if (hasHeightMap)
    {
        float heightSample = texture2D(HeightMap, offset).r;
        offset += E.xy * (heightSample * 0.08 - 0.04);
    }

    vec4 baseMap   = texture2D(BaseMap, offset);
    vec4 normalMap = texture2D(NormalMap, offset);
    vec4 glowTex   = texture2D(GlowMap, offset);

    vec3 normalTS = normalize(normalMap.rgb * 2.0 - 1.0);

    vec3 L = normalize(vLightDir);
    vec3 H = normalize(L + E);

    float NdotL    = max(dot(normalTS, L), 0.0);
    float NdotH    = max(dot(normalTS, H), 0.0);
    float EdotN    = max(dot(normalTS, E), 0.0);
    float NdotNegL = max(dot(normalTS, -L), 0.0);

    vec3 reflectedTS = reflect(-E, normalTS);

    vec3 reflectedVS = (vBitangent * reflectedTS.x)
    + (vTangent   * reflectedTS.y)
    + (vNormal    * reflectedTS.z);

    vec3 reflectedWS = vec3(worldMatrix *
    (modelViewMatrixInverse * vec4(reflectedVS, 0.0)));

    vec3 albedo  = baseMap.rgb * vVertexColor.rgb;
    vec3 diffuse = vAmbientColor.rgb + (vDiffuseColor.rgb * NdotL);

    if (hasCubeMap)
    {
        vec4 cubeSample = textureCube(CubeMap, reflectedWS);
        cubeSample.rgb *= envReflection;

        if (hasEnvMask)
        {
            vec4 envMask = texture2D(EnvironmentMap, offset);
            cubeSample.rgb *= envMask.r;
        }
        else
        {
            cubeSample.rgb *= normalMap.a;
        }

        albedo += cubeSample.rgb;
    }

    vec3 emissive = vec3(0.0);

    if (hasEmit)
    {
        emissive += glowColor * glowMult;

        if (hasGlowMap)
        {
            emissive *= glowTex.rgb;
        }
    }

    float specFactor = pow(NdotH, specGlossiness);
    vec3 spec = clamp(specColor * specStrength * normalMap.a * specFactor, 0.0, 1.0);
    spec *= vDiffuseColor.rgb;

    if (hasBacklight)
    {
        vec3 backTex = texture2D(BacklightMap, offset).rgb;
        backTex *= NdotNegL;
        emissive += backTex * vDiffuseColor.rgb;
    }

    vec4 mask = vec4(0.0);
    if (hasRimlight || hasSoftlight)
    {
        mask = texture2D(LightMask, offset);
    }

    if (hasRimlight)
    {
        vec3 rim = mask.rgb * pow((1.0 - EdotN), rimPower);
        rim *= smoothstep(-0.2, 1.0, dot(-L, E));
        emissive += rim * vDiffuseColor.rgb;
    }

    if (hasSoftlight)
    {
        float wrap = (dot(normalTS, L) + softlight) / (1.0 + softlight);
        vec3 soft = max(wrap, 0.0) * mask.rgb * smoothstep(1.0, 0.0, NdotL);
        soft *= sqrt(clamp(softlight, 0.0, 1.0));

        emissive += soft * vDiffuseColor.rgb;
    }

    if (hasTintColor)
    {
        albedo *= tintColor;
    }

    vec3 finalColor = albedo * (diffuse + emissive) + spec;

    float finalAlpha = vVertexColor.a * baseMap.a * alpha;

    finalColor = tonemap(finalColor) / tonemap(vec3(1.0));

    vec4 outColor = vec4(finalColor, finalAlpha);

    gl_FragColor = outColor;
}
