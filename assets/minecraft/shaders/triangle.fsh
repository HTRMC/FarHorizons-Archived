#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) flat in uint fragTextureIndex;

layout(location = 0) out vec4 outColor;

// Bindless texture array
layout(set = 0, binding = 0) uniform sampler2D textures[];

void main() {
    // Sample from bindless texture array
    vec4 texColor = texture(textures[nonuniformEXT(fragTextureIndex)], fragTexCoord);

    // Mix texture color with vertex color for debugging
    outColor = texColor * vec4(fragColor, 1.0);
}