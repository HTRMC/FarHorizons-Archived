#include "BlockModel.hpp"
#include "BlockRegistryNew.hpp"
#include <simdjson.h>
#include <spdlog/spdlog.h>
#include <fstream>
#include <filesystem>
#include <unordered_set>

namespace VoxelEngine {

// Helper function to normalize model/texture names
// Converts "minecraft:block/stone" or "block/stone" to "stone"
static std::string normalizeResourceName(const std::string& name) {
    std::string result = name;

    // Remove prefixes in order
    const std::vector<std::string> prefixes = {
        "minecraft:block/",
        "minecraft:blocks/",
        "minecraft:",
        "block/"
    };

    for (const auto& prefix : prefixes) {
        if (result.find(prefix) == 0) {
            result = result.substr(prefix.length());
            break;  // Only remove the first matching prefix
        }
    }

    return result;
}

// JSON parsing helpers
namespace JsonHelpers {

    // Safely get a string from a JSON element
    inline bool getString(const simdjson::dom::element& elem, std::string& out) {
        std::string_view sv;
        if (!elem.get(sv)) {
            out = std::string(sv);
            return true;
        }
        return false;
    }

    // Safely get a double from a JSON element
    inline bool getDouble(const simdjson::dom::element& elem, double& out) {
        return !elem.get(out);
    }

    // Parse a vec3 from a JSON array [x, y, z]
    inline bool parseVec3(const simdjson::dom::element& elem, glm::vec3& out) {
        simdjson::dom::array arr;
        if (elem.get(arr)) return false;

        size_t i = 0;
        for (auto coord : arr) {
            double val;
            if (!coord.get(val) && i < 3) {
                out[i++] = static_cast<float>(val);
            }
        }
        return i == 3;
    }

    // Parse a vec4 from a JSON array [x, y, z, w]
    inline bool parseVec4(const simdjson::dom::element& elem, glm::vec4& out) {
        simdjson::dom::array arr;
        if (elem.get(arr)) return false;

        size_t i = 0;
        for (auto coord : arr) {
            double val;
            if (!coord.get(val) && i < 4) {
                out[i++] = static_cast<float>(val);
            }
        }
        return i == 4;
    }
}

} // namespace VoxelEngine

namespace VoxelEngine {

void BlockModel::mergeParent(const BlockModel& parentModel) {
    // Merge textures (parent textures don't override existing ones)
    for (const auto& [key, value] : parentModel.textures) {
        if (textures.find(key) == textures.end()) {
            textures[key] = value;
        }
    }

    // If we don't have elements, use parent's elements
    if (elements.empty() && !parentModel.elements.empty()) {
        elements = parentModel.elements;
    }
}

std::string BlockModel::resolveTexture(const std::string& textureRef) const {
    // If it's a variable reference (starts with #), resolve it
    if (textureRef.size() > 0 && textureRef[0] == '#') {
        std::string varName = textureRef.substr(1);
        auto it = textures.find(varName);
        if (it != textures.end()) {
            // Recursively resolve in case the texture points to another variable
            return resolveTexture(it->second);
        }
        spdlog::warn("Could not resolve texture variable: {}", textureRef);
        return textureRef;
    }
    return textureRef;
}

BlockModelManager::BlockModelManager() {
}

void BlockModelManager::initialize() {
    m_assetsPath = "assets";
    spdlog::info("Initializing BlockModelManager with assets path: {}", m_assetsPath);
}

const BlockModel* BlockModelManager::loadModel(const std::string& modelName) {
    // Parse namespace and path from model name
    std::string namespaceName = "minecraft"; // Default namespace
    std::string modelPath = modelName;

    // Check if there's a namespace prefix (e.g., "minecraft:block/stone")
    size_t colonPos = modelName.find(':');
    if (colonPos != std::string::npos) {
        namespaceName = modelName.substr(0, colonPos);
        modelPath = modelName.substr(colonPos + 1);
    }

    // Normalize for cache key (without namespace)
    std::string normalizedName = normalizeResourceName(modelName);

    // Check if already loaded
    auto it = m_models.find(normalizedName);
    if (it != m_models.end()) {
        return it->second.get();
    }

    // Construct file path: assets/{namespace}/models/{path}.json
    std::string fullPath = m_assetsPath + "/" + namespaceName + "/models/" + modelPath + ".json";

    // Load the model
    auto model = loadModelFromFile(fullPath);
    if (!model) {
        spdlog::error("Failed to load model: {} (path: {})", modelName, fullPath);
        return nullptr;
    }

    // Store the model
    BlockModel* modelPtr = model.get();
    m_models[normalizedName] = std::move(model);

    // Resolve parent hierarchy
    resolveModel(modelPtr);

    spdlog::debug("Loaded model: {} from {}", normalizedName, fullPath);
    return modelPtr;
}

std::unique_ptr<BlockModel> BlockModelManager::loadModelFromFile(const std::string& modelPath) {
    // Check if file exists
    if (!std::filesystem::exists(modelPath)) {
        spdlog::error("Model file not found: {}", modelPath);
        return nullptr;
    }

    // Read file
    std::ifstream file(modelPath);
    if (!file.is_open()) {
        spdlog::error("Failed to open model file: {}", modelPath);
        return nullptr;
    }

    std::string jsonContent((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
    file.close();

    // Parse JSON
    simdjson::dom::parser parser;
    simdjson::dom::element doc;
    auto error = parser.parse(jsonContent).get(doc);
    if (error) {
        spdlog::error("Failed to parse JSON for model: {}", modelPath);
        return nullptr;
    }

    auto model = std::make_unique<BlockModel>();

    // Parse parent
    simdjson::dom::element parentElem;
    if (!doc["parent"].get(parentElem)) {
        std::string_view parentStr;
        if (!parentElem.get(parentStr)) {
            model->parent = std::string(parentStr);
        }
    }

    // Parse textures
    simdjson::dom::element texturesElem;
    if (!doc["textures"].get(texturesElem)) {
        simdjson::dom::object texturesObj;
        if (!texturesElem.get(texturesObj)) {
            for (auto [key, value] : texturesObj) {
                std::string_view keyStr = key;
                std::string_view valueStr;
                if (!value.get(valueStr)) {
                    model->textures[std::string(keyStr)] = std::string(valueStr);
                }
            }
        }
    }

    // Parse elements
    simdjson::dom::element elementsElem;
    if (!doc["elements"].get(elementsElem)) {
        simdjson::dom::array elementsArr;
        if (!elementsElem.get(elementsArr)) {
            for (simdjson::dom::element elementElem : elementsArr) {
                BlockElement element;

                // Parse from
                simdjson::dom::array fromArr;
                if (!elementElem["from"].get(fromArr)) {
                    size_t i = 0;
                    for (simdjson::dom::element coord : fromArr) {
                        double val;
                        if (!coord.get(val) && i < 3) {
                            element.from[i++] = static_cast<float>(val);
                        }
                    }
                }

                // Parse to
                simdjson::dom::array toArr;
                if (!elementElem["to"].get(toArr)) {
                    size_t i = 0;
                    for (simdjson::dom::element coord : toArr) {
                        double val;
                        if (!coord.get(val) && i < 3) {
                            element.to[i++] = static_cast<float>(val);
                        }
                    }
                }

                // Parse faces
                simdjson::dom::element facesElem;
                if (!elementElem["faces"].get(facesElem)) {
                    simdjson::dom::object facesObj;
                    if (!facesElem.get(facesObj)) {
                        for (auto [faceKey, faceValue] : facesObj) {
                            std::string_view faceKeyStr = faceKey;
                            auto faceDir = parseFaceDirection(std::string(faceKeyStr));
                            if (!faceDir) continue;

                            BlockFace face;

                            // Default UVs to full texture (0, 0, 16, 16) if not specified
                            face.uv = glm::vec4(0.0f, 0.0f, 16.0f, 16.0f);

                            // Parse UV (override defaults if present)
                            simdjson::dom::array uvArr;
                            if (!faceValue["uv"].get(uvArr)) {
                                size_t i = 0;
                                for (simdjson::dom::element uvCoord : uvArr) {
                                    double val;
                                    if (!uvCoord.get(val) && i < 4) {
                                        face.uv[i++] = static_cast<float>(val);
                                    }
                                }
                            }

                            // Parse texture
                            std::string_view textureStr;
                            if (!faceValue["texture"].get(textureStr)) {
                                face.texture = std::string(textureStr);
                            }

                            // Parse cullface (optional)
                            std::string_view cullfaceStr;
                            if (!faceValue["cullface"].get(cullfaceStr)) {
                                face.cullface = parseFaceDirection(std::string(cullfaceStr));
                            }

                            element.faces[*faceDir] = face;
                        }
                    }
                }

                model->elements.push_back(element);
            }
        }
    }

    return model;
}

void BlockModelManager::resolveModel(BlockModel* model) {
    if (!model || model->isResolved) {
        return;
    }

    // If this model has a parent, load and resolve it first
    if (model->parent.has_value()) {
        const BlockModel* parentModel = loadModel(model->parent.value());
        if (parentModel) {
            model->mergeParent(*parentModel);
        } else {
            spdlog::warn("Could not load parent model: {}", model->parent.value());
        }
    }

    model->isResolved = true;
}

void BlockModelManager::registerTexture(const std::string& textureName, uint32_t textureIndex) {
    std::string normalized = normalizeTextureName(textureName);
    m_textureMap[normalized] = textureIndex;
    spdlog::debug("Registered texture '{}' with index {}", normalized, textureIndex);
}

uint32_t BlockModelManager::getTextureIndex(const std::string& textureName) const {
    std::string normalized = normalizeTextureName(textureName);
    auto it = m_textureMap.find(normalized);
    if (it != m_textureMap.end()) {
        return it->second;
    }
    spdlog::warn("Texture '{}' not found in texture map, using default index 0", normalized);
    return 0;
}

std::string BlockModelManager::normalizeTextureName(const std::string& textureName) const {
    return normalizeResourceName(textureName);
}

std::vector<std::string> BlockModelManager::getAllTextureNames() const {
    std::unordered_set<std::string> uniqueTextures;

    // Extract textures from all models that are actually used by blockstates
    for (const auto& [stateId, model] : m_stateToModel) {
        if (!model || !model->isResolved) {
            continue;
        }

        // Go through all elements and their faces
        for (const auto& element : model->elements) {
            for (const auto& [faceDir, face] : element.faces) {
                // Resolve the texture reference
                std::string resolvedTexture = model->resolveTexture(face.texture);

                // Normalize and add to set
                std::string normalized = normalizeTextureName(resolvedTexture);
                if (!normalized.empty() && normalized[0] != '#') {
                    // Only add if it's a real texture path (not an unresolved reference)
                    uniqueTextures.insert(normalized);
                }
            }
        }
    }

    // Convert set to vector
    return std::vector<std::string>(uniqueTextures.begin(), uniqueTextures.end());
}

std::unordered_map<std::string, std::string> BlockModelManager::loadBlockstatesFile(const std::string& blockName) {
    std::unordered_map<std::string, std::string> variantToModel;

    // Construct path: assets/minecraft/blockstates/{blockName}.json
    std::string blockstatesPath = m_assetsPath + "/minecraft/blockstates/" + blockName + ".json";

    // Check if file exists
    if (!std::filesystem::exists(blockstatesPath)) {
        spdlog::debug("Blockstates file not found: {} (block has no properties)", blockstatesPath);
        return variantToModel;
    }

    // Read file
    std::ifstream file(blockstatesPath);
    if (!file.is_open()) {
        spdlog::error("Failed to open blockstates file: {}", blockstatesPath);
        return variantToModel;
    }

    std::string jsonContent((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
    file.close();

    // Parse JSON
    simdjson::dom::parser parser;
    simdjson::dom::element doc;
    auto error = parser.parse(jsonContent).get(doc);
    if (error) {
        spdlog::error("Failed to parse blockstates JSON: {}", blockstatesPath);
        return variantToModel;
    }

    // Parse variants
    simdjson::dom::element variantsElem;
    if (!doc["variants"].get(variantsElem)) {
        simdjson::dom::object variantsObj;
        if (!variantsElem.get(variantsObj)) {
            for (auto [variantKey, variantValue] : variantsObj) {
                std::string_view variantKeyStr = variantKey;
                std::string variantStr(variantKeyStr);

                // Get model name from variant value
                std::string_view modelName;
                if (!variantValue["model"].get(modelName)) {
                    variantToModel[variantStr] = std::string(modelName);
                }
            }
        }
    }

    spdlog::debug("Loaded {} variants from blockstates/{}.json", variantToModel.size(), blockName);
    return variantToModel;
}

void BlockModelManager::preloadBlockStateModels() {
    spdlog::info("Preloading blockstate models...");

    // Preload models for AIR (always null)
    m_stateToModel[0] = nullptr;

    // Iterate through all registered blocks
    for (const auto& [blockName, block] : BlockRegistryNew::getAllBlocks()) {
        auto properties = block->getProperties();
        auto variants = loadBlockstatesFile(block->m_name);

        if (variants.empty() && properties.empty()) {
            // Simple block with no properties and no blockstates file
            // Use default model: block/{blockName}.json
            for (size_t i = 0; i < block->getStateCount(); ++i) {
                uint16_t stateId = block->m_baseStateId + i;
                const BlockModel* model = loadModel("block/" + block->m_name);
                m_stateToModel[stateId] = model;
                if (model) {
                    spdlog::trace("Cached model for state {} ({})", stateId, block->m_name);
                }
            }
        } else if (!variants.empty() && !properties.empty()) {
            // Block with properties - map variants to states
            for (const auto& [variantKey, modelName] : variants) {
                // Parse variant key like "type=bottom" into property values
                // For single property blocks, the variant key is "property=value"
                size_t eqPos = variantKey.find('=');
                if (eqPos == std::string::npos) {
                    spdlog::warn("Invalid variant key '{}' in blockstates/{}.json", variantKey, block->m_name);
                    continue;
                }

                std::string propName = variantKey.substr(0, eqPos);
                std::string valueName = variantKey.substr(eqPos + 1);

                // Find the property
                PropertyBase* prop = nullptr;
                for (PropertyBase* p : properties) {
                    if (p->name == propName) {
                        prop = p;
                        break;
                    }
                }

                if (!prop) {
                    spdlog::warn("Property '{}' not found in block {}", propName, block->m_name);
                    continue;
                }

                // Get state index from property value
                auto stateIndexOpt = prop->getValueIndexByName(valueName);
                if (!stateIndexOpt) {
                    spdlog::warn("Value '{}' not found in property '{}'", valueName, propName);
                    continue;
                }

                size_t stateIndex = *stateIndexOpt;
                uint16_t stateId = block->m_baseStateId + stateIndex;

                // Load and cache the model
                const BlockModel* model = loadModel(modelName);
                m_stateToModel[stateId] = model;
                if (model) {
                    spdlog::trace("Cached model for state {} ({} -> {})", stateId, variantKey, modelName);
                } else {
                    spdlog::warn("Failed to load model for state {} ({})", stateId, modelName);
                }
            }
        }
    }

    spdlog::info("Preloaded {} blockstate models", m_stateToModel.size());
}

const BlockModel* BlockModelManager::getModelByStateId(uint16_t stateId) const {
    auto it = m_stateToModel.find(stateId);
    if (it != m_stateToModel.end()) {
        return it->second;
    }

    // Not in cache - this shouldn't happen if preloadBlockStateModels was called
    spdlog::warn("Blockstate {} not in model cache", stateId);
    return nullptr;
}

} // namespace VoxelEngine