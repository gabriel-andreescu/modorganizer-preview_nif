#version 120

uniform mat4 mvpMatrix;

attribute vec3 position;
attribute vec4 color;

varying vec4 LineColor;

void main(void)
{
    gl_Position = mvpMatrix * vec4(position, 1.0);
    LineColor = color;
}
