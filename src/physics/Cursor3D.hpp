#pragma once

namespace FarHorizon {

// Cursor3D class (from Minecraft)
// Iterates through a 3D box of positions with special type tracking
class Cursor3D {
public:
    // Type constants (Minecraft: public static final int)
    static constexpr int TYPE_INSIDE = 0;  // Not on any boundary
    static constexpr int TYPE_FACE = 1;    // On exactly one boundary face
    static constexpr int TYPE_EDGE = 2;    // On exactly two boundary edges
    static constexpr int TYPE_CORNER = 3;  // On all three boundaries (corner)

    // Constructor (Minecraft: Cursor3D(int minX, int minY, int minZ, int maxX, int maxY, int maxZ))
    Cursor3D(int minX, int minY, int minZ, int maxX, int maxY, int maxZ);

    // Advance to next position (Minecraft: public boolean advance())
    bool advance();

    // Get current X coordinate (Minecraft: public int nextX())
    int nextX() const;

    // Get current Y coordinate (Minecraft: public int nextY())
    int nextY() const;

    // Get current Z coordinate (Minecraft: public int nextZ())
    int nextZ() const;

    // Get type of current position (Minecraft: public int getNextType())
    // Returns 0-3 based on how many boundaries the position is on
    int getNextType() const;

private:
    int originX_;
    int originY_;
    int originZ_;
    int width_;
    int height_;
    int depth_;
    int end_;
    int index_;
    int x_;
    int y_;
    int z_;
};

} // namespace FarHorizon