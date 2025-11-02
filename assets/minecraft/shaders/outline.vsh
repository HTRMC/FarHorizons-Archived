#version 450

layout(location = 0) in vec3 inPosition;

layout(push_constant) uniform PushConstants {
    mat4 viewProj;
    ivec3 cameraPositionInteger;
    float _pad0;
    vec3 cameraPositionFraction;
    float _pad1;
} pushConstants;

void main() {
    // Apply camera-relative positioning
    vec3 cameraPos = vec3(pushConstants.cameraPositionInteger) + pushConstants.cameraPositionFraction;
    vec3 worldPos = inPosition - cameraPos;

    gl_Position = pushConstants.viewProj * vec4(worldPos, 1.0);
}