#version 120

uniform mat4 modelViewMatrix;
uniform mat4 mvpMatrix;
uniform mat3 normalMatrix;
uniform vec3 lightDirection;
uniform vec4 ambientColor;
uniform vec4 diffuseColor;

attribute vec3 position;
attribute vec3 normal;
attribute vec3 tangent;
attribute vec3 bitangent;
attribute vec2 texCoord;
attribute vec4 color;

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

void main(void)
{
    gl_Position = mvpMatrix * vec4(position, 1.0);

    vTexCoord = texCoord;

    vec3 N = normalize(normalMatrix * normal);
    vec3 T = normalize(normalMatrix * tangent);
    vec3 B = normalize(normalMatrix * bitangent);

    vec3 viewPos = (modelViewMatrix * vec4(position, 1.0)).xyz;
    vPosViewSpace = viewPos;

    mat3 tbnMatrix = mat3(
    B.x, T.x, N.x,
    B.y, T.y, N.y,
    B.z, T.z, N.z
    );

    vViewDir = tbnMatrix * (-viewPos);

    vLightDir = tbnMatrix * lightDirection;

    vAmbientColor = ambientColor;
    vDiffuseColor = diffuseColor;
    vVertexColor  = color;

    vNormal    = N;
    vTangent   = T;
    vBitangent = B;
}
