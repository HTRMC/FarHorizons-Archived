#include "VulkanContext.hpp"
#include <spdlog/spdlog.h>
#include <GLFW/glfw3.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

namespace FarHorizon {

VulkanContext::VulkanContext(VulkanContext&& other) noexcept
    : instance_(other.instance_)
    , surface_(other.surface_)
    , debugMessenger_(std::move(other.debugMessenger_))
    , device_(std::move(other.device_))
    , allocator_(other.allocator_)
{
    other.instance_ = VK_NULL_HANDLE;
    other.surface_ = VK_NULL_HANDLE;
    other.allocator_ = VK_NULL_HANDLE;
}

VulkanContext& VulkanContext::operator=(VulkanContext&& other) noexcept {
    if (this != &other) {
        shutdown();

        instance_ = other.instance_;
        surface_ = other.surface_;
        debugMessenger_ = std::move(other.debugMessenger_);
        device_ = std::move(other.device_);
        allocator_ = other.allocator_;

        other.instance_ = VK_NULL_HANDLE;
        other.surface_ = VK_NULL_HANDLE;
        other.allocator_ = VK_NULL_HANDLE;
    }
    return *this;
}

void VulkanContext::init(GLFWwindow* window, const std::string& appName) {
    spdlog::info("[VulkanContext] Initializing...");

    createInstance(appName);
    debugMessenger_.init(instance_);
    createSurface(window);
    device_.pickPhysicalDevice(instance_, surface_);
    device_.createLogicalDevice();
    createAllocator();

    spdlog::info("[VulkanContext] Initialization complete");
}

void VulkanContext::shutdown() {
    if (instance_ != VK_NULL_HANDLE) {
        waitIdle();

        if (allocator_ != VK_NULL_HANDLE) {
            vmaDestroyAllocator(allocator_);
            spdlog::info("[VulkanContext] VMA allocator destroyed");
            allocator_ = VK_NULL_HANDLE;
        }

        device_.shutdown();

        if (surface_ != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(instance_, surface_, nullptr);
            spdlog::info("[VulkanContext] Surface destroyed");
            surface_ = VK_NULL_HANDLE;
        }

        debugMessenger_.shutdown(instance_);

        vkDestroyInstance(instance_, nullptr);
        spdlog::info("[VulkanContext] Instance destroyed");
        instance_ = VK_NULL_HANDLE;
    }
}

void VulkanContext::waitIdle() const {
    if (device_.getLogicalDevice() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_.getLogicalDevice());
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

    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &instance_));
    spdlog::info("[VulkanContext] Instance created");
}

void VulkanContext::createSurface(GLFWwindow* window) {
    VK_CHECK(glfwCreateWindowSurface(instance_, window, nullptr, &surface_));
    spdlog::info("[VulkanContext] Surface created");
}

void VulkanContext::createAllocator() {
    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    allocatorInfo.instance = instance_;
    allocatorInfo.physicalDevice = device_.getPhysicalDevice();
    allocatorInfo.device = device_.getLogicalDevice();

    VK_CHECK(vmaCreateAllocator(&allocatorInfo, &allocator_));
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

} // namespace FarHorizon