#version 120

uniform sampler2D BaseMap;
uniform sampler2D GreyscaleMap;

uniform bool doubleSided;

uniform bool hasSourceTexture;
uniform bool hasGreyscaleMap;
uniform bool greyscaleAlpha;
uniform bool greyscaleColor;

uniform bool useFalloff;
uniform bool vertexColors;
uniform bool vertexAlpha;

uniform bool hasWeaponBlood;

uniform vec4 glowColor;
uniform float glowMult;

uniform vec2 uvScale;
uniform vec2 uvOffset;

uniform vec4 falloffParams;
uniform float falloffDepth;

uniform float alphaThreshold;
uniform int alphaTestMode;

varying vec2 TexCoord;
varying vec3 LightDir;
varying vec3 ViewDir;

varying vec4 C;

varying vec3 N;
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
    vec4 baseMap = vec4( 1.0 );
    if ( hasSourceTexture ) {
        baseMap = texture2D( BaseMap, TexCoord.st * uvScale + uvOffset );
    }

    vec4 color;

    vec3 normal = N;

    // Reconstructed normal
    //normal = normalize(cross(dFdy(v.xyz), dFdx(v.xyz)));

    //if ( !doubleSided && !gl_FrontFacing ) { return; }

    vec3 E = normalize(ViewDir);

    float tmp2 = falloffDepth; // Unused right now

    // Skyrim computes falloff this way rather than with smoothstep, so a start angle above the stop angle stays
    // defined, and it leaves the opacities unclamped.
    float falloff = 1.0;
    if ( useFalloff ) {
        float t = clamp( (abs(E.b) - falloffParams.x) / (falloffParams.y - falloffParams.x), 0.0, 1.0 );
        falloff = mix( falloffParams.z, falloffParams.w, t * t * (3.0 - 2.0 * t) );
    }

    float alphaMult = glowColor.a;

    color.rgb = baseMap.rgb;
    color.a = baseMap.a;

    if ( hasWeaponBlood ) {
        color.rgb = vec3( 1.0, 0.0, 0.0 ) * baseMap.r;
        color.a = baseMap.a * baseMap.g;
    }

    color.rgb *= C.rgb * glowColor.rgb;
    color.a *= C.a * falloff * alphaMult;

    // Skyrim picks the palette row from the base color's and vertex color's red channels, without falloff.
    if ( hasGreyscaleMap && greyscaleColor ) {
        color.rgb = colorLookup( baseMap.g, glowColor.r * C.r ).rgb;
    }

    if ( hasGreyscaleMap && greyscaleAlpha ) {
        vec4 luA = colorLookup( baseMap.a, C.a * falloff * alphaMult );

        color.a = luA.a;
    }

    if (!passesAlphaTest(color.a)) {
        discard;
    }

    gl_FragColor.rgb = color.rgb * glowMult;
    gl_FragColor.a = color.a;
}
