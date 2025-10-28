#version 450

// Vertex attributes from buffer
layout(location = 0) in vec2 inPosition;      // NDC position (-1 to 1)
layout(location = 1) in vec2 inTexCoord;      // UV coordinates
layout(location = 2) in vec4 inColor;         // Text color
layout(location = 3) in uint inTextureIndex;  // Font texture index

// Output to fragment shader
layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) flat out uint fragTextureIndex;

void main() {
    // Position is already in NDC space, so no transformation needed
    gl_Position = vec4(inPosition, 0.0, 1.0);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
    fragTextureIndex = inTextureIndex;
}