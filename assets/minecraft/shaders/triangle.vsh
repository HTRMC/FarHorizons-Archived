#version 460

// Push constants for camera view-projection matrix and camera position
layout(push_constant) uniform PushConstants {
    mat4 viewProj;
    ivec3 cameraPositionInteger;  // Integer part of camera position
    float _pad0;
    vec3 cameraPositionFraction;   // Fractional part (0.0-1.0)
    float _pad1;
} pc;

// Compact face data (per-face data in SSBO instead of vertex attributes)
struct FaceData {
    uint packed1;  // Position (bits 0-14), isBackFace (bit 15), lightIndex (bits 16-31)
    uint packed2;  // quadIndex (texture is in QuadInfo)
};

layout(std430, set = 1, binding = 3) readonly buffer FaceDataBuffer {
    FaceData faces[];
};

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
    ivec3 position;   // Chunk world position in blocks (chunkX * 16, chunkY * 16, chunkZ * 16)
    uint faceOffset;  // Offset into FaceData buffer (also used for lighting buffer - same index)
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
    // Get chunk ID from indirect draw command
    uint chunkID = gl_BaseInstance;

    // Get chunk metadata
    ChunkData chunk = chunks[chunkID];

    // Calculate face index: chunk's face offset + instance offset (0, 1, 2, ...)
    uint faceIndex = chunk.faceOffset + (gl_InstanceIndex - gl_BaseInstance);

    // Fetch FaceData from SSBO
    FaceData faceData = faces[faceIndex];

    // Unpack FaceData
    uint x = faceData.packed1 & 0x1Fu;
    uint y = (faceData.packed1 >> 5) & 0x1Fu;
    uint z = (faceData.packed1 >> 10) & 0x1Fu;
    bool isBackFace = ((faceData.packed1 >> 15) & 0x1u) != 0u;
    uint lightIndex = (faceData.packed1 >> 16) & 0xFFFFu;  // Global lighting buffer index
    uint quadIndex = faceData.packed2 & 0xFFFFu;  // Quad index is in lower 16 bits

    // Get quad geometry (includes texture)
    QuadInfo quad = quadInfos[quadIndex];

    // Get lighting data from separate lighting buffer using global index
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

    // Build world-space position
    // 1. Start with local block position within chunk (0-31)
    vec3 position = vec3(float(x), float(y), float(z));

    // 2. Add corner offset (0.0-1.0)
    position += localCorner;

    // 3. Add chunk world position to get absolute world coordinates
    position += vec3(chunk.position);

    // 4. Convert to camera-relative position for floating-point precision
    // Reconstruct full camera position: integer part + fractional part
    vec3 cameraPosition = vec3(pc.cameraPositionInteger) + pc.cameraPositionFraction;

    // Transform to camera-relative coordinates (keeps positions near origin)
    vec3 cameraRelativePos = position - cameraPosition;

    // 5. Transform to clip space with camera-relative position
    gl_Position = pc.viewProj * vec4(cameraRelativePos, 1.0);

    // Unpack lighting into color
    vec3 lightColor = unpackLighting(cornerLight);

    // Apply diffuse shading based on face normal (Minecraft-style)
    float diffuse = 1.0;
    vec3 normal = quad.normal;
    if (abs(normal.y) > 0.9) {
        // Top/Bottom faces
        diffuse = (normal.y > 0.0) ? 1.0 : 0.5;  // Top: 100%, Bottom: 50%
    } else if (abs(normal.z) > 0.9) {
        // North/South faces
        diffuse = 0.8;
    } else if (abs(normal.x) > 0.9) {
        // East/West faces
        diffuse = 0.6;
    }

    fragColor = lightColor * diffuse;
    fragTexCoord = uv;
    fragTextureIndex = quad.textureSlot;  // Texture is now in QuadInfo, not FaceData
}