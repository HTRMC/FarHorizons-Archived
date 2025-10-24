#include "VulkanContext.hpp"
#include <spdlog/spdlog.h>
#include <GLFW/glfw3.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

namespace VoxelEngine {

VulkanContext::VulkanContext(VulkanContext&& other) noexcept
    : m_instance(other.m_instance)
    , m_surface(other.m_surface)
    , m_debugMessenger(std::move(other.m_debugMessenger))
    , m_device(std::move(other.m_device))
    , m_allocator(other.m_allocator)
{
    other.m_instance = VK_NULL_HANDLE;
    other.m_surface = VK_NULL_HANDLE;
    other.m_allocator = VK_NULL_HANDLE;
}

VulkanContext& VulkanContext::operator=(VulkanContext&& other) noexcept {
    if (this != &other) {
        shutdown();

        m_instance = other.m_instance;
        m_surface = other.m_surface;
        m_debugMessenger = std::move(other.m_debugMessenger);
        m_device = std::move(other.m_device);
        m_allocator = other.m_allocator;

        other.m_instance = VK_NULL_HANDLE;
        other.m_surface = VK_NULL_HANDLE;
        other.m_allocator = VK_NULL_HANDLE;
    }
    return *this;
}

void VulkanContext::init(GLFWwindow* window, const std::string& appName) {
    spdlog::info("[VulkanContext] Initializing...");

    createInstance(appName);
    m_debugMessenger.init(m_instance);
    createSurface(window);
    m_device.pickPhysicalDevice(m_instance, m_surface);
    m_device.createLogicalDevice();
    createAllocator();

    spdlog::info("[VulkanContext] Initialization complete");
}

void VulkanContext::shutdown() {
    if (m_instance != VK_NULL_HANDLE) {
        waitIdle();

        if (m_allocator != VK_NULL_HANDLE) {
            vmaDestroyAllocator(m_allocator);
            spdlog::info("[VulkanContext] VMA allocator destroyed");
            m_allocator = VK_NULL_HANDLE;
        }

        m_device.shutdown();

        if (m_surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
            spdlog::info("[VulkanContext] Surface destroyed");
            m_surface = VK_NULL_HANDLE;
        }

        m_debugMessenger.shutdown(m_instance);

        vkDestroyInstance(m_instance, nullptr);
        spdlog::info("[VulkanContext] Instance destroyed");
        m_instance = VK_NULL_HANDLE;
    }
}

void VulkanContext::waitIdle() const {
    if (m_device.getLogicalDevice() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_device.getLogicalDevice());
    }
}

void VulkanContext::createInstance(const std::string& appName) {
    // Check validation layer support
    if (!checkValidationLayerSupport()) {
        spdlog::error("[VulkanContext] Validation layers requested but not available!");
        assert(false);
    }

    // Application info
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = appName.c_str();
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Voxel Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3; // Vulkan 1.3 for dynamic rendering

    // Instance create info
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    auto extensions = getRequiredExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    auto validationLayers = getRequiredValidationLayers();
    createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
    createInfo.ppEnabledLayerNames = validationLayers.data();

    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &m_instance));
    spdlog::info("[VulkanContext] Instance created");
}

void VulkanContext::createSurface(GLFWwindow* window) {
    VK_CHECK(glfwCreateWindowSurface(m_instance, window, nullptr, &m_surface));
    spdlog::info("[VulkanContext] Surface created");
}

void VulkanContext::createAllocator() {
    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    allocatorInfo.instance = m_instance;
    allocatorInfo.physicalDevice = m_device.getPhysicalDevice();
    allocatorInfo.device = m_device.getLogicalDevice();

    VK_CHECK(vmaCreateAllocator(&allocatorInfo, &m_allocator));
    spdlog::info("[VulkanContext] VMA allocator created");
}

std::vector<const char*> VulkanContext::getRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    // Add debug utils extension
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    spdlog::info("[VulkanContext] Required extensions:");
    for (const char* ext : extensions) {
        spdlog::info("  - {}", ext);
    }

    return extensions;
}

} // namespace VoxelEngine