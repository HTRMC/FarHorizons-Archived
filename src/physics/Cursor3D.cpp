#include "Cursor3D.hpp"

namespace FarHorizon {

// Constructor (Minecraft: Cursor3D(int minX, int minY, int minZ, int maxX, int maxY, int maxZ))
Cursor3D::Cursor3D(int minX, int minY, int minZ, int maxX, int maxY, int maxZ)
    : originX_(minX)
    , originY_(minY)
    , originZ_(minZ)
    , width_(maxX - minX + 1)
    , height_(maxY - minY + 1)
    , depth_(maxZ - minZ + 1)
    , end_(width_ * height_ * depth_)
    , index_(0)
    , x_(0)
    , y_(0)
    , z_(0)
{
}

// Advance to next position (Minecraft: public boolean advance())
bool Cursor3D::advance() {
    // Check if we've reached the end (Minecraft: if (this.index == this.end))
    if (index_ == end_) {
        return false;
    }

    // Convert linear index to 3D coordinates (Minecraft: lines 34-37)
    x_ = index_ % width_;
    int var1 = index_ / width_;
    y_ = var1 % height_;
    z_ = var1 / height_;

    // Increment index for next call (Minecraft: ++this.index)
    ++index_;

    return true;
}

// Get current X coordinate (Minecraft: public int nextX())
int Cursor3D::nextX() const {
    return originX_ + x_;
}

// Get current Y coordinate (Minecraft: public int nextY())
int Cursor3D::nextY() const {
    return originY_ + y_;
}

// Get current Z coordinate (Minecraft: public int nextZ())
int Cursor3D::nextZ() const {
    return originZ_ + z_;
}

// Get type of current position (Minecraft: public int getNextType())
int Cursor3D::getNextType() const {
    int var1 = 0;

    // Count how many boundaries this position is on
    // (Minecraft: lines 57-67)
    if (x_ == 0 || x_ == width_ - 1) {
        ++var1;
    }

    if (y_ == 0 || y_ == height_ - 1) {
        ++var1;
    }

    if (z_ == 0 || z_ == depth_ - 1) {
        ++var1;
    }

    return var1;
}

} // namespace FarHorizon