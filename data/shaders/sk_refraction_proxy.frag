#version 120

uniform sampler2D BaseMap;
uniform sampler2D SceneMap;
uniform vec2 viewportSize;
uniform vec2 uvScale;
uniform vec2 uvOffset;
uniform float refractionStrength;

varying vec2 TexCoord;

void main(void)
{
    vec2 sceneUv = gl_FragCoord.xy / viewportSize;
    vec2 proxyUv = TexCoord * uvScale + uvOffset;
    vec4 distortion = texture2D(BaseMap, proxyUv);

    vec2 direction = distortion.xy * 2.0 - 1.0;
    float strength = clamp(refractionStrength, 0.0, 1.0);
    float offsetPixels = min(strength * 80.0, 12.0);
    vec2 offset = direction * offsetPixels * distortion.a / viewportSize;

    vec4 original = texture2D(SceneMap, sceneUv);
    vec4 shifted = texture2D(SceneMap, clamp(sceneUv + offset, vec2(0.001), vec2(0.999)));

    float blend = clamp(distortion.a * strength * 2.25, 0.0, 0.38);
    gl_FragColor = vec4(mix(original.rgb, shifted.rgb, blend), 1.0);
}
