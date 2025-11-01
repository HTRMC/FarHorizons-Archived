#version 460

layout(location = 0) in vec4 fragColor;
layout(location = 0) out vec4 outColor;

void main() {
    // Simply output the interpolated color
    outColor = fragColor;
}