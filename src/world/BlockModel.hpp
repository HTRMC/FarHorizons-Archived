#pragma once

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
    uint32_t textureIndex = 0;  // Cached texture index (set during preloading)
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

    // Initialize block model system
    void initialize();

    // Load a specific model by name (e.g., "block/stone")
    const BlockModel* loadModel(const std::string& modelName);

    // Register a texture name to index mapping
    void registerTexture(const std::string& textureName, uint32_t textureIndex);

    // Get texture index by name (returns 0 if not found)
    uint32_t getTextureIndex(const std::string& textureName) const;

    // Get all unique texture names referenced by loaded models
    std::vector<std::string> getAllTextureNames() const;

    // Preload all blockstate models and cache them
    void preloadBlockStateModels();

    // Cache texture indices in all loaded models (call after texture registration)
    void cacheTextureIndices();

    // Get a model by blockstate ID (fast cached lookup)
    const BlockModel* getModelByStateId(uint16_t stateId) const;

private:
    std::string m_assetsPath;  // Base assets path (e.g., "assets")
    std::unordered_map<std::string, std::unique_ptr<BlockModel>> m_models;
    std::unordered_map<std::string, uint32_t> m_textureMap;
    std::unordered_map<uint16_t, const BlockModel*> m_stateToModel;  // Blockstate ID -> Model cache

    // Load model from JSON file
    std::unique_ptr<BlockModel> loadModelFromFile(const std::string& modelPath);

    // Load blockstates JSON file and return variant -> model name mapping
    std::unordered_map<std::string, std::string> loadBlockstatesFile(const std::string& blockName);

    // Resolve parent hierarchy for a model
    void resolveModel(BlockModel* model);

    // Normalize texture names (remove minecraft: prefix, etc.)
    std::string normalizeTextureName(const std::string& textureName) const;
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