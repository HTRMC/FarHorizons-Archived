#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>

namespace FarHorizon {

// SPIR-V shader module wrapper
class Shader {
public:
    Shader() = default;
    ~Shader() { cleanup(); }

    // No copy
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    // Move semantics
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    // Load from SPIR-V file
    void loadFromFile(VkDevice device, const std::string& filepath);

    // Load from SPIR-V code
    void loadFromCode(VkDevice device, const std::vector<uint32_t>& code);

    void cleanup();

    VkShaderModule getModule() const { return m_module; }

private:
    std::vector<uint32_t> readFile(const std::string& filepath);

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkShaderModule m_module = VK_NULL_HANDLE;
};

} // namespace FarHorizon