#version 450

// Push constants for camera view-projection matrix
layout(push_constant) uniform PushConstants {
    mat4 viewProj;
} pc;

// Hardcoded 3D triangle vertices (world space)
vec3 positions[3] = vec3[](
    vec3(0.0, 0.5, 0.0),    // Top
    vec3(0.5, -0.5, 0.0),   // Bottom right
    vec3(-0.5, -0.5, 0.0)   // Bottom left
);

// RGB colors for each vertex
vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),  // Red
    vec3(0.0, 1.0, 0.0),  // Green
    vec3(0.0, 0.0, 1.0)   // Blue
);

layout(location = 0) out vec3 fragColor;

void main() {
    // Transform vertex position from world space to clip space
    gl_Position = pc.viewProj * vec4(positions[gl_VertexIndex], 1.0);
    fragColor = colors[gl_VertexIndex];
}