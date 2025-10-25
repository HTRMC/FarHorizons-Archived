#pragma once

#include "BlockType.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <memory>

namespace VoxelEngine {

// Face direction enum for cullface
enum class FaceDirection {
    DOWN,
    UP,
    NORTH,
    SOUTH,
    WEST,
    EAST
};

// Represents a single face of a block element
struct BlockFace {
    glm::vec4 uv;  // UV coordinates (minU, minV, maxU, maxV)
    std::string texture;  // Texture variable (e.g., "#side")
    std::optional<FaceDirection> cullface;  // Which face to cull against
};

// Represents a cuboid element of a block model
struct BlockElement {
    glm::vec3 from;  // Starting corner (in 0-16 space)
    glm::vec3 to;    // Ending corner (in 0-16 space)
    std::unordered_map<FaceDirection, BlockFace> faces;
};

// Represents a complete block model
class BlockModel {
public:
    BlockModel() = default;

    // Parent model name (e.g., "block/block")
    std::optional<std::string> parent;

    // Texture variables (e.g., "side" -> "minecraft:blocks/stone")
    std::unordered_map<std::string, std::string> textures;

    // Model elements (cuboids)
    std::vector<BlockElement> elements;

    // Whether this model has been fully resolved (parent merged)
    bool isResolved = false;

    // Merge parent model data into this model
    void mergeParent(const BlockModel& parentModel);

    // Resolve texture variables (replace #variable with actual texture)
    std::string resolveTexture(const std::string& textureRef) const;
};

// Manages loading and caching of block models
class BlockModelManager {
public:
    BlockModelManager();

    // Initialize and load all block models
    void initialize(const std::string& modelsPath);

    // Get a block model by block type
    const BlockModel* getModel(BlockType type) const;

    // Load a specific model by name (e.g., "block/stone")
    const BlockModel* loadModel(const std::string& modelName);

private:
    std::string m_modelsPath;
    std::unordered_map<std::string, std::unique_ptr<BlockModel>> m_models;
    std::unordered_map<BlockType, std::string> m_blockToModel;

    // Load model from JSON file
    std::unique_ptr<BlockModel> loadModelFromFile(const std::string& modelPath);

    // Resolve parent hierarchy for a model
    void resolveModel(BlockModel* model);
};

// Helper function to convert string to FaceDirection
inline std::optional<FaceDirection> parseFaceDirection(const std::string& str) {
    if (str == "down") return FaceDirection::DOWN;
    if (str == "up") return FaceDirection::UP;
    if (str == "north") return FaceDirection::NORTH;
    if (str == "south") return FaceDirection::SOUTH;
    if (str == "west") return FaceDirection::WEST;
    if (str == "east") return FaceDirection::EAST;
    return std::nullopt;
}

inline const char* faceDirectionToString(FaceDirection dir) {
    switch (dir) {
        case FaceDirection::DOWN: return "down";
        case FaceDirection::UP: return "up";
        case FaceDirection::NORTH: return "north";
        case FaceDirection::SOUTH: return "south";
        case FaceDirection::WEST: return "west";
        case FaceDirection::EAST: return "east";
    }
    return "unknown";
}

} // namespace VoxelEngine