#pragma once

#include <glm/glm.hpp>

namespace FarHorizon {

// SymmetricGroup3 - axis permutations (from Minecraft's SymmetricGroup3.java)
enum class SymmetricGroup3 {
    P123,  // (X, Y, Z) - identity
    P213,  // (Y, X, Z) - swap X and Y
    P132,  // (X, Z, Y) - swap Y and Z
    P312,  // (Z, X, Y) - rotate
    P231,  // (Y, Z, X) - rotate
    P321   // (Z, Y, X) - swap X and Z
};

// Helper to permute coordinates based on SymmetricGroup3
inline void permuteCoords(SymmetricGroup3 perm, float& x, float& y, float& z) {
    float temp[3] = {x, y, z};

    switch (perm) {
        case SymmetricGroup3::P123:  // Identity
            break;
        case SymmetricGroup3::P213:  // (Y, X, Z)
            x = temp[1]; y = temp[0]; z = temp[2];
            break;
        case SymmetricGroup3::P132:  // (X, Z, Y)
            x = temp[0]; y = temp[2]; z = temp[1];
            break;
        case SymmetricGroup3::P312:  // (Z, X, Y)
            x = temp[2]; y = temp[0]; z = temp[1];
            break;
        case SymmetricGroup3::P231:  // (Y, Z, X)
            x = temp[1]; y = temp[2]; z = temp[0];
            break;
        case SymmetricGroup3::P321:  // (Z, Y, X)
            x = temp[2]; y = temp[1]; z = temp[0];
            break;
    }
}

// OctahedralGroup - 48 symmetries of a cube (from Minecraft's OctahedralGroup.java)
// Each transformation is an axis permutation + optional inversions
struct OctahedralGroup {
    SymmetricGroup3 permutation;
    bool invertX;
    bool invertY;
    bool invertZ;

    // Apply transformation to coordinates
    glm::vec3 transform(const glm::vec3& v) const {
        float x = v.x;
        float y = v.y;
        float z = v.z;

        // Apply permutation
        permuteCoords(permutation, x, y, z);

        // Apply inversions
        if (invertX) x = 1.0f - x;
        if (invertY) y = 1.0f - y;
        if (invertZ) z = 1.0f - z;

        return glm::vec3(x, y, z);
    }

    // Common transformations (from OctahedralGroup.java lines 16-43, 66-73)
    static const OctahedralGroup IDENTITY;
    static const OctahedralGroup BLOCK_ROT_Y_90;   // 90° CCW around Y
    static const OctahedralGroup BLOCK_ROT_Y_180;  // 180° around Y
    static const OctahedralGroup BLOCK_ROT_Y_270;  // 270° CCW around Y (= 90° CW)
    static const OctahedralGroup INVERT_Y;         // Vertical flip
};

// Define common transformations (matching Minecraft's static definitions)
inline const OctahedralGroup OctahedralGroup::IDENTITY = {
    SymmetricGroup3::P123, false, false, false
};

// BLOCK_ROT_Y_90 = ROT_90_Y_NEG (line 70)
// ROT_90_Y_NEG = SymmetricGroup3.P321, invertX=true, invertY=false, invertZ=false (line 36)
inline const OctahedralGroup OctahedralGroup::BLOCK_ROT_Y_90 = {
    SymmetricGroup3::P321, true, false, false
};

// BLOCK_ROT_Y_180 = ROT_180_FACE_XZ (line 69)
// ROT_180_FACE_XZ = SymmetricGroup3.P123, invertX=true, invertY=false, invertZ=true (line 18)
inline const OctahedralGroup OctahedralGroup::BLOCK_ROT_Y_180 = {
    SymmetricGroup3::P123, true, false, true
};

// BLOCK_ROT_Y_270 = ROT_90_Y_POS (line 68)
// ROT_90_Y_POS = SymmetricGroup3.P321, invertX=false, invertY=false, invertZ=true (line 37)
inline const OctahedralGroup OctahedralGroup::BLOCK_ROT_Y_270 = {
    SymmetricGroup3::P321, false, false, true
};

// INVERT_Y (line 42)
// INVERT_Y = SymmetricGroup3.P123, invertX=false, invertY=true, invertZ=false (line 42)
inline const OctahedralGroup OctahedralGroup::INVERT_Y = {
    SymmetricGroup3::P123, false, true, false
};

} // namespace FarHorizon