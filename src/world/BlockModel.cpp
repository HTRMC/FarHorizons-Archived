#include "BlockModel.hpp"
#include "BlockRegistry.hpp"
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

                            // Parse tintindex (optional)
                            int64_t tintindexValue;
                            if (!faceValue["tintindex"].get(tintindexValue)) {
                                face.tintindex = static_cast<int>(tintindexValue);
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

std::unordered_map<std::string, BlockModelManager::VariantData> BlockModelManager::loadBlockstatesFile(const std::string& blockName) {
    std::unordered_map<std::string, VariantData> variantToData;

    // Construct path: assets/minecraft/blockstates/{blockName}.json
    std::string blockstatesPath = m_assetsPath + "/minecraft/blockstates/" + blockName + ".json";

    // Check if file exists
    if (!std::filesystem::exists(blockstatesPath)) {
        spdlog::debug("Blockstates file not found: {} (block has no properties)", blockstatesPath);
        return variantToData;
    }

    // Read file
    std::ifstream file(blockstatesPath);
    if (!file.is_open()) {
        spdlog::error("Failed to open blockstates file: {}", blockstatesPath);
        return variantToData;
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
        return variantToData;
    }

    // Parse variants
    simdjson::dom::element variantsElem;
    if (!doc["variants"].get(variantsElem)) {
        simdjson::dom::object variantsObj;
        if (!variantsElem.get(variantsObj)) {
            for (auto [variantKey, variantValue] : variantsObj) {
                std::string_view variantKeyStr = variantKey;
                std::string variantStr(variantKeyStr);

                // Variant value can be either:
                // 1. A single object: { "model": "...", "x": 90, "y": 180, "uvlock": true }
                // 2. An array of objects: [{ "model": "..." }, { "model": "...", "y": 90 }, ...]
                // For arrays, we use the first element

                VariantData data;
                simdjson::dom::array variantArray;
                simdjson::dom::element variantObj;

                if (!variantValue.get(variantArray)) {
                    // It's an array - use the first element
                    variantObj = variantArray.at(0);
                } else {
                    // It's a single object
                    variantObj = variantValue;
                }

                // Parse model name (required)
                std::string_view modelName;
                if (!variantObj["model"].get(modelName)) {
                    data.modelName = std::string(modelName);
                } else {
                    spdlog::warn("Variant {} missing 'model' field", variantStr);
                    continue;
                }

                // Parse x rotation (optional)
                int64_t xRot;
                if (!variantObj["x"].get(xRot)) {
                    data.rotationX = static_cast<int>(xRot);
                }

                // Parse y rotation (optional)
                int64_t yRot;
                if (!variantObj["y"].get(yRot)) {
                    data.rotationY = static_cast<int>(yRot);
                }

                // Parse uvlock (optional)
                bool uvlockVal;
                if (!variantObj["uvlock"].get(uvlockVal)) {
                    data.uvlock = uvlockVal;
                }

                variantToData[variantStr] = data;
            }
        }
    }

    spdlog::debug("Loaded {} variants from blockstates/{}.json", variantToData.size(), blockName);
    return variantToData;
}

void BlockModelManager::preloadBlockStateModels() {
    spdlog::info("Preloading blockstate models...");

    // Iterate through all registered blocks
    for (const auto& [blockName, block] : BlockRegistry::getAllBlocks()) {
        // Skip blocks with INVISIBLE render type (air, barriers, etc.)
        if (block->getRenderType(block->getDefaultState()) == BlockRenderType::INVISIBLE) {
            // Cache null for all states of invisible blocks
            for (size_t i = 0; i < block->getStateCount(); ++i) {
                uint16_t stateId = block->m_baseStateId + i;
                m_stateToModel[stateId] = nullptr;
            }
            spdlog::debug("Skipped model loading for invisible block: {}", block->m_name);
            continue;
        }

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
        } else if (!variants.empty() && properties.empty()) {
            // Block with variants but no properties (like stone with empty "" variant)
            // Use first variant model for all states
            if (!variants.empty()) {
                const BlockModel* model = loadModel(variants.begin()->second.modelName);
                for (size_t i = 0; i < block->getStateCount(); ++i) {
                    uint16_t stateId = block->m_baseStateId + i;
                    m_stateToModel[stateId] = model;
                    if (model) {
                        spdlog::trace("Cached model for state {} ({} -> {})", stateId, block->m_name, variants.begin()->second.modelName);
                    }
                }
            }
        } else if (!variants.empty() && !properties.empty()) {
            // Block with properties - map variants to states
            for (const auto& [variantKey, variantData] : variants) {
                // Parse variant key like "facing=east,half=top,shape=straight" into property values
                // Split by comma to get individual property=value pairs
                std::unordered_map<std::string, std::string> propValues;

                size_t start = 0;
                size_t comma = variantKey.find(',');
                while (true) {
                    std::string pair = (comma == std::string::npos)
                        ? variantKey.substr(start)
                        : variantKey.substr(start, comma - start);

                    size_t eqPos = pair.find('=');
                    if (eqPos != std::string::npos) {
                        std::string propName = pair.substr(0, eqPos);
                        std::string valueName = pair.substr(eqPos + 1);
                        propValues[propName] = valueName;
                    }

                    if (comma == std::string::npos) break;
                    start = comma + 1;
                    comma = variantKey.find(',', start);
                }

                // Calculate state index from property values
                // State index is calculated as: value0 + value1 * size0 + value2 * size0 * size1 + ...
                size_t stateIndex = 0;
                size_t multiplier = 1;
                bool allPropertiesFound = true;

                for (PropertyBase* prop : properties) {
                    auto it = propValues.find(prop->name);
                    if (it == propValues.end()) {
                        spdlog::warn("Property '{}' not found in variant key '{}' for block {}",
                                     prop->name, variantKey, block->m_name);
                        allPropertiesFound = false;
                        break;
                    }

                    auto valueIndexOpt = prop->getValueIndexByName(it->second);
                    if (!valueIndexOpt) {
                        spdlog::warn("Value '{}' not found in property '{}' for block {}",
                                     it->second, prop->name, block->m_name);
                        allPropertiesFound = false;
                        break;
                    }

                    stateIndex += (*valueIndexOpt) * multiplier;
                    multiplier *= prop->getNumValues();
                }

                if (!allPropertiesFound) {
                    continue;
                }

                uint16_t stateId = block->m_baseStateId + stateIndex;

                // Load and cache the model with rotation data
                const BlockModel* model = loadModel(variantData.modelName);
                m_stateToModel[stateId] = model;

                // Store variant data with rotation info
                BlockStateVariant variant;
                variant.model = model;
                variant.rotationX = variantData.rotationX;
                variant.rotationY = variantData.rotationY;
                variant.uvlock = variantData.uvlock;
                m_stateToVariant[stateId] = variant;

                if (model) {
                    spdlog::trace("Cached model for state {} ({} -> {} with rot x={} y={})",
                                  stateId, variantKey, variantData.modelName, variantData.rotationX, variantData.rotationY);
                } else {
                    spdlog::warn("Failed to load model for state {} ({})", stateId, variantData.modelName);
                }
            }
        }
    }

    spdlog::info("Preloaded {} blockstate models", m_stateToModel.size());
}

const BlockStateVariant* BlockModelManager::getVariantByStateId(uint16_t stateId) const {
    auto it = m_stateToVariant.find(stateId);
    if (it != m_stateToVariant.end()) {
        return &(it->second);
    }

    // Not in variant cache - this block may not have rotation data
    return nullptr;
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

void BlockModelManager::cacheTextureIndices() {
    spdlog::info("Caching texture indices in block models...");

    size_t totalFacesCached = 0;
    std::unordered_set<const BlockModel*> processedModels;

    // Only cache texture indices for models actually used by blockstates
    // (not parent/template models which may have unresolved texture variables)
    for (const auto& [stateId, model] : m_stateToModel) {
        if (!model || !model->isResolved) {
            continue;
        }

        // Skip if we've already processed this model
        if (processedModels.find(model) != processedModels.end()) {
            continue;
        }
        processedModels.insert(model);

        // Process each element in the model (need to cast away const to modify)
        BlockModel* mutableModel = const_cast<BlockModel*>(model);
        for (auto& element : mutableModel->elements) {
            // Process each face of the element
            for (auto& [faceDir, face] : element.faces) {
                // Resolve the texture reference (e.g., "#side" -> "minecraft:blocks/stone")
                std::string resolvedTexture = model->resolveTexture(face.texture);

                // Get and cache the texture index
                face.textureIndex = getTextureIndex(resolvedTexture);
                totalFacesCached++;
            }
        }
    }

    spdlog::info("Cached texture indices for {} faces across {} unique models",
                 totalFacesCached, processedModels.size());
}

} // namespace VoxelEngine