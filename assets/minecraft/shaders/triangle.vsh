#version 450

// Push constants for camera view-projection matrix
layout(push_constant) uniform PushConstants {
    mat4 viewProj;
} pc;

// Vertex attributes from buffer
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

// Output to fragment shader
layout(location = 0) out vec3 fragColor;

void main() {
    // Transform vertex position from world space to clip space
    gl_Position = pc.viewProj * vec4(inPosition, 1.0);
    fragColor = inColor;
}