#pragma once
#include "../Block.hpp"
#include "../Properties.hpp"
#include "../BlockShape.hpp"
#include "enums/BlockHalf.hpp"
#include "enums/StairFacing.hpp"
#include "enums/StairShape.hpp"
#include "util/OctahedralGroup.hpp"
#include "util/Direction.hpp"
#include "physics/BlockGetter.hpp"
#include <map>
#include <spdlog/spdlog.h>

namespace FarHorizon {

// Forward declaration to avoid circular dependency
class BlockRegistry;

// StairBlock - stair block with facing, half, and shape properties
// Based on Minecraft's StairBlock.java with static pre-computed shapes
class StairBlock : public Block {
public:
    StairBlock(const std::string& name) : Block(name) {
        // Shapes will be initialized on first access
    }

    // Stairs are never fully opaque on any face
    bool isFaceOpaque(BlockState state, Face face) const override {
        return false;
    }

    bool isFullCube() const override {
        return false;  // Stairs aren't full cubes
    }

    bool isSolid() const override {
        return true;  // Stairs are solid
    }

    // State count automatically calculated: 4 facings × 2 halves × 5 shapes = 40

    // Expose properties for model loading
    // Order: FACING, HALF, SHAPE (determines state index calculation)
    std::vector<PropertyBase*> getProperties() const override {
        return {
            const_cast<PropertyBase*>(static_cast<const PropertyBase*>(&Properties::STAIR_FACING)),
            const_cast<PropertyBase*>(static_cast<const PropertyBase*>(&Properties::BLOCK_HALF)),
            const_cast<PropertyBase*>(static_cast<const PropertyBase*>(&Properties::STAIR_SHAPE))
        };
    }

    // Helper to get specific state
    // State index = facing + half * 4 + shape * 8
    BlockState withFacingHalfAndShape(StairFacing facing, BlockHalf half, StairShape shape) const {
        uint16_t offset = static_cast<uint16_t>(facing) +
                         static_cast<uint16_t>(half) * 4 +
                         static_cast<uint16_t>(shape) * 8;
        return BlockState(baseStateId_ + offset);
    }

    // Helper for simple placement (always straight shape for now)
    BlockState withFacingAndHalf(StairFacing facing, BlockHalf half) const {
        return withFacingHalfAndShape(facing, half, StairShape::STRAIGHT);
    }

    // Helper: Check if stair can take shape in a direction (Minecraft StairBlock.java line 156-159)
    // Returns true if the neighbor in the given direction allows this stair to form a corner
    static bool canTakeShape(BlockState state, const BlockGetter& level, const glm::ivec3& pos, StairFacing neighbour);

    // Helper: Check if a block state is a stair (Minecraft StairBlock.java line 161-163)
    static bool isStairs(BlockState state);

    // Get stair shape based on neighbors (Minecraft StairBlock.java line 127-154)
    static StairShape getStairsShape(BlockState state, const BlockGetter& level, const glm::ivec3& pos);

    // Override outline shape for stairs
    // Based on Minecraft's StairBlock.java getShape() method (lines 67-105)
    BlockShape getOutlineShape(BlockState state) const override {
        // Initialize static shapes on first access
        try {
            initializeStaticShapes();
        } catch (const std::exception& e) {
            spdlog::error("StairBlock: Failed to initialize shapes: {}", e.what());
            return BlockShape::fullCube();  // Fallback to full cube
        }

        BlockHalf half = getHalf(state);
        StairFacing facing = getFacing(state);
        StairShape shape = getShape(state);

        // Select the appropriate shape map based on half
        const std::map<StairFacing, BlockShape>* shapeMap = nullptr;

        switch (shape) {
            case StairShape::STRAIGHT:
                shapeMap = (half == BlockHalf::BOTTOM) ? &SHAPE_BOTTOM_STRAIGHT : &SHAPE_TOP_STRAIGHT;
                break;
            case StairShape::OUTER_LEFT:
            case StairShape::OUTER_RIGHT:
                shapeMap = (half == BlockHalf::BOTTOM) ? &SHAPE_BOTTOM_OUTER : &SHAPE_TOP_OUTER;
                break;
            case StairShape::INNER_LEFT:
            case StairShape::INNER_RIGHT:
                shapeMap = (half == BlockHalf::BOTTOM) ? &SHAPE_BOTTOM_INNER : &SHAPE_TOP_INNER;
                break;
        }

        // Determine the facing direction for the shape lookup
        // This matches Minecraft's logic (lines 87-102)
        StairFacing lookupFacing;
        switch (shape) {
            case StairShape::STRAIGHT:
            case StairShape::OUTER_LEFT:
            case StairShape::INNER_RIGHT:
                lookupFacing = facing;  // Use facing directly
                break;
            case StairShape::INNER_LEFT:
                // Rotate counter-clockwise (Minecraft: var6.getCounterClockWise())
                lookupFacing = HorizontalDirection::getCounterClockWise(facing);
                break;
            case StairShape::OUTER_RIGHT:
                // Rotate clockwise (Minecraft: var6.getClockWise())
                lookupFacing = HorizontalDirection::getClockWise(facing);
                break;
        }

        auto it = shapeMap->find(lookupFacing);
        if (it == shapeMap->end()) {
            spdlog::error("StairBlock: Shape not found for facing {}", static_cast<int>(lookupFacing));
            return BlockShape::fullCube();  // Fallback
        }

        BlockShape result = it->second;

        return result;
    }

private:
    // Decode facing, half, and shape from state ID
    StairFacing getFacing(BlockState state) const {
        int offset = state.id - baseStateId_;
        return static_cast<StairFacing>(offset % 4);
    }

    BlockHalf getHalf(BlockState state) const {
        int offset = state.id - baseStateId_;
        return static_cast<BlockHalf>((offset / 4) % 2);
    }

    StairShape getShape(BlockState state) const {
        int offset = state.id - baseStateId_;
        return static_cast<StairShape>(offset / 8);
    }

    // Static shape initialization (from Minecraft StairBlock.java lines 226-234)
    // Shapes are computed once and reused
    static inline bool shapesInitialized = false;
    static inline BlockShape SHAPE_OUTER;
    static inline BlockShape SHAPE_STRAIGHT;
    static inline BlockShape SHAPE_INNER;
    static inline std::map<StairFacing, BlockShape> SHAPE_BOTTOM_OUTER;
    static inline std::map<StairFacing, BlockShape> SHAPE_BOTTOM_STRAIGHT;
    static inline std::map<StairFacing, BlockShape> SHAPE_BOTTOM_INNER;
    static inline std::map<StairFacing, BlockShape> SHAPE_TOP_OUTER;
    static inline std::map<StairFacing, BlockShape> SHAPE_TOP_STRAIGHT;
    static inline std::map<StairFacing, BlockShape> SHAPE_TOP_INNER;

    // Helper: convert pixel coords (0-16) to normalized (0-1) and create shape
    static BlockShape box(float minX, float minY, float minZ, float maxX, float maxY, float maxZ) {
        return BlockShape::fromBounds(
            glm::vec3(minX / 16.0f, minY / 16.0f, minZ / 16.0f),
            glm::vec3(maxX / 16.0f, maxY / 16.0f, maxZ / 16.0f)
        );
    }

    // Helper: column (centered square column) - matches Minecraft's Block.column()
    static BlockShape column(float sizeXZ, float minY, float maxY) {
        float half = sizeXZ / 2.0f;
        return box(8.0f - half, minY, 8.0f - half, 8.0f + half, maxY, 8.0f + half);
    }

    // Initialize all static shapes (matches Minecraft's static initializer)
    static void initializeStaticShapes() {
        if (shapesInitialized) return;

        spdlog::info("StairBlock: Initializing static shapes...");

        // SHAPE_OUTER = Shapes.or(Block.column(16.0, 0.0, 8.0), Block.box(0.0, 8.0, 0.0, 8.0, 16.0, 8.0));
        BlockShape col = column(16.0f, 0.0f, 8.0f);
        spdlog::info("  column(16, 0, 8): isEmpty={}, bounds=({},{},{}) to ({},{},{})",
                     col.isEmpty(), col.getMin().x, col.getMin().y, col.getMin().z,
                     col.getMax().x, col.getMax().y, col.getMax().z);

        BlockShape topBox = box(0.0f, 8.0f, 0.0f, 8.0f, 16.0f, 8.0f);
        spdlog::info("  box(0,8,0,8,16,8): isEmpty={}, bounds=({},{},{}) to ({},{},{})",
                     topBox.isEmpty(), topBox.getMin().x, topBox.getMin().y, topBox.getMin().z,
                     topBox.getMax().x, topBox.getMax().y, topBox.getMax().z);

        SHAPE_OUTER = BlockShape::unionShapes(col, topBox);
        spdlog::info("  SHAPE_OUTER: isEmpty={}, bounds=({},{},{}) to ({},{},{})",
                     SHAPE_OUTER.isEmpty(), SHAPE_OUTER.getMin().x, SHAPE_OUTER.getMin().y, SHAPE_OUTER.getMin().z,
                     SHAPE_OUTER.getMax().x, SHAPE_OUTER.getMax().y, SHAPE_OUTER.getMax().z);

        // SHAPE_STRAIGHT = Shapes.or(SHAPE_OUTER, Shapes.rotate(SHAPE_OUTER, BLOCK_ROT_Y_90));
        SHAPE_STRAIGHT = BlockShape::unionShapes(
            SHAPE_OUTER,
            SHAPE_OUTER.rotate(OctahedralGroup::BLOCK_ROT_Y_90)
        );
        spdlog::info("  SHAPE_STRAIGHT: isEmpty={}, bounds=({},{},{}) to ({},{},{})",
                     SHAPE_STRAIGHT.isEmpty(), SHAPE_STRAIGHT.getMin().x, SHAPE_STRAIGHT.getMin().y, SHAPE_STRAIGHT.getMin().z,
                     SHAPE_STRAIGHT.getMax().x, SHAPE_STRAIGHT.getMax().y, SHAPE_STRAIGHT.getMax().z);

        // SHAPE_INNER = Shapes.or(SHAPE_STRAIGHT, Shapes.rotate(SHAPE_STRAIGHT, BLOCK_ROT_Y_90));
        SHAPE_INNER = BlockShape::unionShapes(
            SHAPE_STRAIGHT,
            SHAPE_STRAIGHT.rotate(OctahedralGroup::BLOCK_ROT_Y_90)
        );
        spdlog::info("  SHAPE_INNER: isEmpty={}, bounds=({},{},{}) to ({},{},{})",
                     SHAPE_INNER.isEmpty(), SHAPE_INNER.getMin().x, SHAPE_INNER.getMin().y, SHAPE_INNER.getMin().z,
                     SHAPE_INNER.getMax().x, SHAPE_INNER.getMax().y, SHAPE_INNER.getMax().z);

        // Create rotated versions for each horizontal direction
        // SHAPE_BOTTOM_OUTER = Shapes.rotateHorizontal(SHAPE_OUTER);
        SHAPE_BOTTOM_OUTER = rotateHorizontal(SHAPE_OUTER);
        SHAPE_BOTTOM_STRAIGHT = rotateHorizontal(SHAPE_STRAIGHT);
        SHAPE_BOTTOM_INNER = rotateHorizontal(SHAPE_INNER);

        // SHAPE_TOP_* = Shapes.rotateHorizontal(SHAPE_*, INVERT_Y);
        SHAPE_TOP_OUTER = rotateHorizontal(SHAPE_OUTER.rotate(OctahedralGroup::INVERT_Y));
        SHAPE_TOP_STRAIGHT = rotateHorizontal(SHAPE_STRAIGHT.rotate(OctahedralGroup::INVERT_Y));
        SHAPE_TOP_INNER = rotateHorizontal(SHAPE_INNER.rotate(OctahedralGroup::INVERT_Y));

        spdlog::info("StairBlock: Shape initialization complete");
        shapesInitialized = true;
    }

    // Rotate shape for all 4 horizontal directions
    // Matches Minecraft's Shapes.rotateHorizontal()
    static std::map<StairFacing, BlockShape> rotateHorizontal(const BlockShape& baseShape) {
        std::map<StairFacing, BlockShape> result;

        // North (facing=0) - no rotation
        result[StairFacing::NORTH] = baseShape;

        // East (facing=1) - 90° CW = 270° CCW
        result[StairFacing::EAST] = baseShape.rotate(OctahedralGroup::BLOCK_ROT_Y_270);

        // South (facing=2) - 180°
        result[StairFacing::SOUTH] = baseShape.rotate(OctahedralGroup::BLOCK_ROT_Y_180);

        // West (facing=3) - 270° CW = 90° CCW
        result[StairFacing::WEST] = baseShape.rotate(OctahedralGroup::BLOCK_ROT_Y_90);

        return result;
    }
};

} // namespace FarHorizon