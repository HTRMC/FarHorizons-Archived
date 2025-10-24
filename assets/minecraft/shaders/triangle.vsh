#version 450

// Push constants for camera view-projection matrix
layout(push_constant) uniform PushConstants {
    mat4 viewProj;
} pc;

// Vertex attributes from buffer
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in uint inTextureIndex;

// Output to fragment shader
layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) flat out uint fragTextureIndex;

void main() {
    // Transform vertex position from world space to clip space
    gl_Position = pc.viewProj * vec4(inPosition, 1.0);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
    fragTextureIndex = inTextureIndex;
}