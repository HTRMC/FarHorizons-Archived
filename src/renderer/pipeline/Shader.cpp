#include "Shader.hpp"
#include "../core/VulkanDebug.hpp"
#include <fstream>
#include <spdlog/spdlog.h>

namespace VoxelEngine {

Shader::Shader(Shader&& other) noexcept
    : m_device(other.m_device)
    , m_module(other.m_module)
{
    other.m_device = VK_NULL_HANDLE;
    other.m_module = VK_NULL_HANDLE;
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        cleanup();
        m_device = other.m_device;
        m_module = other.m_module;
        other.m_device = VK_NULL_HANDLE;
        other.m_module = VK_NULL_HANDLE;
    }
    return *this;
}

void Shader::loadFromFile(VkDevice device, const std::string& filepath) {
    m_device = device;
    auto code = readFile(filepath);
    loadFromCode(device, code);
    spdlog::info("[Shader] Loaded: {} ({} bytes)", filepath, code.size() * 4);
}

void Shader::loadFromCode(VkDevice device, const std::vector<uint32_t>& code) {
    m_device = device;

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size() * sizeof(uint32_t);
    createInfo.pCode = code.data();

    VK_CHECK(vkCreateShaderModule(m_device, &createInfo, nullptr, &m_module));
}

void Shader::cleanup() {
    if (m_device != VK_NULL_HANDLE && m_module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(m_device, m_module, nullptr);
        m_module = VK_NULL_HANDLE;
    }
}

std::vector<uint32_t> Shader::readFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        spdlog::error("[Shader] Failed to open file: {}", filepath);
        assert(false);
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
    file.close();

    return buffer;
}

} // namespace VoxelEngine