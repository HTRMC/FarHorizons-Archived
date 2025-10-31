#pragma once

#include "BlockModel.hpp"
#include <glm/glm.hpp>

namespace VoxelEngine {

// Face direction utilities
namespace FaceUtils {

// Convert FaceDirection enum to index (0-5)
inline int toIndex(FaceDirection dir) {
    switch (dir) {
        case FaceDirection::SOUTH: return 0;  // +Z
        case FaceDirection::NORTH: return 1;  // -Z
        case FaceDirection::WEST:  return 2;  // -X
        case FaceDirection::EAST:  return 3;  // +X
        case FaceDirection::UP:    return 4;  // +Y
        case FaceDirection::DOWN:  return 5;  // -Y
    }
    return 0;
}

// Convert index to FaceDirection
inline FaceDirection fromIndex(int index) {
    switch (index) {
        case 0: return FaceDirection::SOUTH;
        case 1: return FaceDirection::NORTH;
        case 2: return FaceDirection::WEST;
        case 3: return FaceDirection::EAST;
        case 4: return FaceDirection::UP;
        case 5: return FaceDirection::DOWN;
    }
    return FaceDirection::NORTH;
}

// Face colors for lighting (could be replaced with proper lighting later)
static constexpr glm::vec3 FACE_COLORS[6] = {
    glm::vec3(0.8f, 0.8f, 0.8f),  // South
    glm::vec3(0.8f, 0.8f, 0.8f),  // North
    glm::vec3(0.6f, 0.6f, 0.6f),  // West
    glm::vec3(0.6f, 0.6f, 0.6f),  // East
    glm::vec3(1.0f, 1.0f, 1.0f),  // Up
    glm::vec3(0.5f, 0.5f, 0.5f),  // Down
};

// Face normals
static constexpr glm::vec3 FACE_NORMALS[6] = {
    glm::vec3(0, 0, 1),   // South
    glm::vec3(0, 0, -1),  // North
    glm::vec3(-1, 0, 0),  // West
    glm::vec3(1, 0, 0),   // East
    glm::vec3(0, 1, 0),   // Up
    glm::vec3(0, -1, 0),  // Down
};

// Directional offsets for each face (for neighbor checking)
static constexpr int FACE_DIRS[6][3] = {
    {0, 0, 1},   // South (+Z)
    {0, 0, -1},  // North (-Z)
    {-1, 0, 0},  // West (-X)
    {1, 0, 0},   // East (+X)
    {0, 1, 0},   // Up (+Y)
    {0, -1, 0},  // Down (-Y)
};

// Generate vertices for a face based on element bounds
inline void getFaceVertices(FaceDirection dir, const glm::vec3& elemFrom, const glm::vec3& elemTo, glm::vec3 vertices[4]) {
    switch (dir) {
        case FaceDirection::DOWN:  // -Y
            vertices[0] = glm::vec3(elemFrom.x, elemFrom.y, elemFrom.z);
            vertices[1] = glm::vec3(elemTo.x, elemFrom.y, elemFrom.z);
            vertices[2] = glm::vec3(elemTo.x, elemFrom.y, elemTo.z);
            vertices[3] = glm::vec3(elemFrom.x, elemFrom.y, elemTo.z);
            break;
        case FaceDirection::UP:  // +Y
            vertices[0] = glm::vec3(elemFrom.x, elemTo.y, elemTo.z);
            vertices[1] = glm::vec3(elemTo.x, elemTo.y, elemTo.z);
            vertices[2] = glm::vec3(elemTo.x, elemTo.y, elemFrom.z);
            vertices[3] = glm::vec3(elemFrom.x, elemTo.y, elemFrom.z);
            break;
        case FaceDirection::NORTH:  // -Z
            vertices[0] = glm::vec3(elemTo.x, elemFrom.y, elemFrom.z);
            vertices[1] = glm::vec3(elemFrom.x, elemFrom.y, elemFrom.z);
            vertices[2] = glm::vec3(elemFrom.x, elemTo.y, elemFrom.z);
            vertices[3] = glm::vec3(elemTo.x, elemTo.y, elemFrom.z);
            break;
        case FaceDirection::SOUTH:  // +Z
            vertices[0] = glm::vec3(elemFrom.x, elemFrom.y, elemTo.z);
            vertices[1] = glm::vec3(elemTo.x, elemFrom.y, elemTo.z);
            vertices[2] = glm::vec3(elemTo.x, elemTo.y, elemTo.z);
            vertices[3] = glm::vec3(elemFrom.x, elemTo.y, elemTo.z);
            break;
        case FaceDirection::WEST:  // -X
            vertices[0] = glm::vec3(elemFrom.x, elemFrom.y, elemFrom.z);
            vertices[1] = glm::vec3(elemFrom.x, elemFrom.y, elemTo.z);
            vertices[2] = glm::vec3(elemFrom.x, elemTo.y, elemTo.z);
            vertices[3] = glm::vec3(elemFrom.x, elemTo.y, elemFrom.z);
            break;
        case FaceDirection::EAST:  // +X
            vertices[0] = glm::vec3(elemTo.x, elemFrom.y, elemTo.z);
            vertices[1] = glm::vec3(elemTo.x, elemFrom.y, elemFrom.z);
            vertices[2] = glm::vec3(elemTo.x, elemTo.y, elemFrom.z);
            vertices[3] = glm::vec3(elemTo.x, elemTo.y, elemTo.z);
            break;
    }
}

// Convert UVs from Blockbench/OpenGL (0-16, bottom-left origin) to Vulkan (0-1, top-left origin)
inline void convertUVs(const glm::vec4& uvIn, glm::vec2 uvOut[4]) {
    uvOut[0] = glm::vec2(uvIn[0] / 16.0f, 1.0f - uvIn[1] / 16.0f);
    uvOut[1] = glm::vec2(uvIn[2] / 16.0f, 1.0f - uvIn[1] / 16.0f);
    uvOut[2] = glm::vec2(uvIn[2] / 16.0f, 1.0f - uvIn[3] / 16.0f);
    uvOut[3] = glm::vec2(uvIn[0] / 16.0f, 1.0f - uvIn[3] / 16.0f);
}

// Check if an element face reaches the block boundary (for cullface)
// elemFrom/elemTo are in 0-1 space (already divided by 16)
// Returns true if the face actually touches the edge in the given direction
inline bool faceReachesBoundary(FaceDirection dir, const glm::vec3& elemFrom, const glm::vec3& elemTo) {
    constexpr float EPSILON = 1e-5f;  // Tolerance for floating point comparison

    switch (dir) {
        case FaceDirection::DOWN:  // -Y, check if elemFrom.y is at 0
            return elemFrom.y < EPSILON;
        case FaceDirection::UP:    // +Y, check if elemTo.y is at 1
            return elemTo.y > (1.0f - EPSILON);
        case FaceDirection::NORTH: // -Z, check if elemFrom.z is at 0
            return elemFrom.z < EPSILON;
        case FaceDirection::SOUTH: // +Z, check if elemTo.z is at 1
            return elemTo.z > (1.0f - EPSILON);
        case FaceDirection::WEST:  // -X, check if elemFrom.x is at 0
            return elemFrom.x < EPSILON;
        case FaceDirection::EAST:  // +X, check if elemTo.x is at 1
            return elemTo.x > (1.0f - EPSILON);
    }
    return false;
}

// Get the opposite face direction (for checking neighbor culling)
inline FaceDirection getOppositeFace(FaceDirection dir) {
    switch (dir) {
        case FaceDirection::DOWN: return FaceDirection::UP;
        case FaceDirection::UP: return FaceDirection::DOWN;
        case FaceDirection::NORTH: return FaceDirection::SOUTH;
        case FaceDirection::SOUTH: return FaceDirection::NORTH;
        case FaceDirection::WEST: return FaceDirection::EAST;
        case FaceDirection::EAST: return FaceDirection::WEST;
    }
    return dir;
}

// Get the normal vector for a face direction
inline glm::vec3 getFaceNormal(FaceDirection dir) {
    return FACE_NORMALS[toIndex(dir)];
}

} // namespace FaceUtils

} // namespace VoxelEngine