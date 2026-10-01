#version 120

uniform sampler2D BaseMap;
uniform sampler2D GreyscaleMap;
uniform samplerCube CubeMap;
uniform sampler2D NormalMap;
uniform sampler2D SpecularMap;

uniform bool doubleSided;

uniform bool hasSourceTexture;
uniform bool hasGreyscaleMap;
uniform bool hasCubeMap;
uniform bool hasNormalMap;
uniform bool hasEnvMask;

uniform bool greyscaleAlpha;
uniform bool greyscaleColor;

uniform bool useFalloff;
uniform bool hasRGBFalloff;

uniform bool hasWeaponBlood;

uniform vec4 glowColor;
uniform float glowMult;

uniform vec2 uvScale;
uniform vec2 uvOffset;

uniform vec4 falloffParams;
uniform float falloffDepth;

uniform float lightingInfluence;
uniform float envReflection;

uniform float alphaThreshold;
uniform int alphaTestMode;

uniform mat4 modelViewMatrixInverse;
uniform mat4 worldMatrix;

varying vec2 TexCoord;
varying vec3 LightDir;
varying vec3 ViewDir;

varying vec4 A;
varying vec4 C;
varying vec4 D;

varying vec3 N;
varying vec3 t;
varying vec3 b;
varying vec3 v;

vec4 colorLookup( float x, float y ) {

    return texture2D( GreyscaleMap, vec2( clamp(x, 0.0, 1.0), clamp(y, 0.0, 1.0)) );
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

void main( void )
{
    vec2 offset = TexCoord * uvScale + uvOffset;

    vec4 baseMap = texture2D( BaseMap, offset );
    vec4 normalMap = texture2D( NormalMap, offset );

    vec3 normal = normalize(normalMap.rgb * 2.0 - 1.0);
    // Calculate missing blue channel
    normal.b = sqrt(1.0 - dot(normal.rg, normal.rg));
    if ( !gl_FrontFacing && doubleSided ) {
        normal *= -1.0;
    }

    vec3 L = normalize(LightDir);
    vec3 V = normalize(ViewDir);
    vec3 R = reflect(-L, normal);
    vec3 H = normalize( L + V );

    float NdotL = max( dot(normal, L), 0.000001 );
    float NdotH = max( dot(normal, H), 0.000001 );
    float NdotV = max( dot(normal, V), 0.000001 );
    float LdotH = max( dot(L, H), 0.000001 );
    float NdotNegL = max( dot(normal, -L), 0.000001 );

    vec3 reflected = reflect( V, normal );
    vec3 reflectedVS = b * reflected.x + t * reflected.y + N * reflected.z;
    vec3 reflectedWS = vec3( worldMatrix * (modelViewMatrixInverse * vec4( reflectedVS, 0.0 )) );

    vec4 baseColor = glowColor;

    // Fallout 4 computes falloff this way rather than with smoothstep, so a start angle above the stop angle stays
    // defined, and it leaves the opacities unclamped.
    float falloff = 1.0;
    if ( useFalloff || hasRGBFalloff ) {
        float t = clamp( (abs(dot(normal, V)) - falloffParams.x) / (falloffParams.y - falloffParams.x), 0.0, 1.0 );
        falloff = mix( falloffParams.z, falloffParams.w, t * t * (3.0 - 2.0 * t) );
    }

    // Fallout 4 converts vertex alpha to linear before using it as opacity.
    vec4 color;
    color.rgb = baseMap.rgb * C.rgb * baseColor.rgb * glowMult;
    color.a = baseMap.a * baseColor.a * pow( C.a, 2.2 );
    if ( useFalloff )
        color.a *= falloff;
    if ( hasRGBFalloff )
        color.rgb *= falloff;

    // Palette rows come from the base color, vertex color and falloff. The texture supplies the column.
    if ( hasGreyscaleMap && greyscaleColor ) {
        color.rgb = colorLookup( baseMap.g, baseColor.r * C.r * falloff ).rgb * glowMult;
    }
    if ( hasGreyscaleMap && greyscaleAlpha ) {
        color.a = colorLookup( baseMap.a, baseColor.a * C.a * falloff ).a;
    }

    vec3 diffuse = A.rgb + (D.rgb * NdotL);
    color.rgb = mix( color.rgb, color.rgb * D.rgb, lightingInfluence );

    // Specular
    float g = 1.0;
    float s = 1.0;
    if ( hasEnvMask ) {
        vec4 specMap = texture2D( SpecularMap, offset );
        g = specMap.r;
        s = specMap.g;
    }

    // Environment
    if ( hasCubeMap ) {
        vec4 cube = textureCube( CubeMap, reflectedWS );
        cube.rgb *= envReflection * s;
        cube.rgb = mix( cube.rgb, cube.rgb * D.rgb, lightingInfluence );

        color.rgb += cube.rgb * falloff;
    }

    if (!passesAlphaTest(color.a)) {
        discard;
    }

    gl_FragColor.rgb = color.rgb;
    gl_FragColor.a = color.a;
}
