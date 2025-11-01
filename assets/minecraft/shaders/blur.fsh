#version 460
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

// Bindless texture array
layout(set = 0, binding = 0) uniform sampler2D textures[];

// Push constant for texture index and blur parameters
layout(push_constant) uniform PushConstants {
    uint textureIndex;
    vec2 blurDir;      // Blur direction (1,0) for horizontal, (0,1) for vertical
    float radius;      // Blur radius
    float _pad;
} pushConstants;

// Minecraft-style separable blur
// Uses linear sampling to reduce texture samples by half
void main() {
    vec2 texelSize = 1.0 / vec2(textureSize(textures[nonuniformEXT(pushConstants.textureIndex)], 0));
    vec2 sampleStep = texelSize * pushConstants.blurDir;

    vec4 blurred = vec4(0.0);
    float actualRadius = pushConstants.radius >= 0.5 ? round(pushConstants.radius) : 8.0;

    // Sample with step of 2 using linear filtering to reduce samples
    for (float a = -actualRadius + 0.5; a <= actualRadius; a += 2.0) {
        blurred += texture(textures[nonuniformEXT(pushConstants.textureIndex)], fragTexCoord + sampleStep * a);
    }

    // Add final half-weighted sample
    blurred += texture(textures[nonuniformEXT(pushConstants.textureIndex)], fragTexCoord + sampleStep * actualRadius) / 2.0;

    outColor = blurred / (actualRadius + 0.5);
}