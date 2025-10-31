#version 460

// Push constants for camera view-projection matrix and camera position
layout(push_constant) uniform PushConstants {
    mat4 viewProj;
    ivec3 cameraPositionInteger;  // Integer part of camera position
    float _pad0;
    vec3 cameraPositionFraction;   // Fractional part (0.0-1.0)
    float _pad1;
} pc;

// Compact face data (per-instance vertex attributes)
layout(location = 0) in uint inPacked1;
layout(location = 1) in uint inPacked2;

// QuadInfo buffer (shared geometry)
struct QuadInfo {
    vec3 normal;
    float _pad0;
    vec3 corner0;
    float _pad1;
    vec3 corner1;
    float _pad2;
    vec3 corner2;
    float _pad3;
    vec3 corner3;
    float _pad4;
    vec2 uv0;
    vec2 uv1;
    vec2 uv2;
    vec2 uv3;
    uint textureSlot;
    uint _pad5;
};

layout(std430, set = 1, binding = 0) readonly buffer QuadInfoBuffer {
    QuadInfo quadInfos[];
};

// Lighting buffer (per-face lighting)
layout(std430, set = 1, binding = 1) readonly buffer LightingBuffer {
    uvec4 lighting[];  // 4 uint32_t per face
};

// ChunkData buffer (per-chunk metadata, indexed by gl_BaseInstance)
struct ChunkData {
    ivec3 position;  // Chunk world position in blocks (chunkX * 16, chunkY * 16, chunkZ * 16)
    int voxelSize;   // Block size (1 for now, could support LOD later)
};

layout(std430, set = 1, binding = 2) readonly buffer ChunkDataBuffer {
    ChunkData chunks[];
};

// Output to fragment shader
layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) flat out uint fragTextureIndex;

// Unpack 5-bit lighting channel
float unpack5bit(uint packed, uint shift) {
    return float((packed >> shift) & 0x1Fu) / 31.0;
}

// Unpack lighting for a corner
vec3 unpackLighting(uint packed) {
    float sunR = unpack5bit(packed, 25);
    float sunG = unpack5bit(packed, 20);
    float sunB = unpack5bit(packed, 15);
    float blockR = unpack5bit(packed, 10);
    float blockG = unpack5bit(packed, 5);
    float blockB = unpack5bit(packed, 0);

    // Combine sun and block lighting
    return vec3(sunR + blockR, sunG + blockG, sunB + blockB);
}

void main() {
    // Get chunk ID from indirect draw command (gl_BaseInstance)
    uint chunkID = gl_BaseInstance;


    // Unpack FaceData
    uint x = inPacked1 & 0x1Fu;
    uint y = (inPacked1 >> 5) & 0x1Fu;
    uint z = (inPacked1 >> 10) & 0x1Fu;
    bool isBackFace = ((inPacked1 >> 15) & 0x1u) != 0u;
    uint lightIndex = (inPacked1 >> 16) & 0xFFFFu;
    uint textureIndex = inPacked2 & 0xFFFFu;
    uint quadIndex = (inPacked2 >> 16) & 0xFFFFu;

    // Get quad geometry
    QuadInfo quad = quadInfos[quadIndex];

    // Get lighting data for this face
    uvec4 faceLighting = lighting[lightIndex];

    // Determine which corner based on gl_VertexIndex (0-5 for two triangles)
    // Triangle 1: 0, 1, 2 (counter-clockwise)
    // Triangle 2: 0, 2, 3 (counter-clockwise, shares edge 0-2 with triangle 1)
    uint cornerIndices[6] = uint[6](0, 1, 2, 0, 2, 3);
    uint cornerIndex = cornerIndices[gl_VertexIndex];

    // Select corner data
    vec3 localCorner;
    vec2 uv;
    uint cornerLight;

    if (cornerIndex == 0u) {
        localCorner = quad.corner0;
        uv = quad.uv0;
        cornerLight = faceLighting.x;
    } else if (cornerIndex == 1u) {
        localCorner = quad.corner1;
        uv = quad.uv1;
        cornerLight = faceLighting.y;
    } else if (cornerIndex == 2u) {
        localCorner = quad.corner2;
        uv = quad.uv2;
        cornerLight = faceLighting.z;
    } else {
        localCorner = quad.corner3;
        uv = quad.uv3;
        cornerLight = faceLighting.w;
    }

    // Build position step-by-step with camera-relative coordinates for precision
    // 1. Start with local block position within chunk (0-31)
    vec3 position = vec3(float(x), float(y), float(z));

    // 2. Add corner offset (0.0-1.0)
    position += localCorner;

    // 3. Scale by voxel size (1 for normal blocks)
    position *= float(chunks[chunkID].voxelSize);

    // 4. Transform to camera-relative world space (precision trick)
    // Add chunk world position, subtract camera position to keep values near zero
    position += vec3(chunks[chunkID].position) - vec3(pc.cameraPositionInteger);
    position -= pc.cameraPositionFraction;

    // 5. Transform to clip space
    gl_Position = pc.viewProj * vec4(position, 1.0);

    // Unpack lighting into color
    fragColor = unpackLighting(cornerLight);
    fragTexCoord = uv;
    fragTextureIndex = textureIndex;
}