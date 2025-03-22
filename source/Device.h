#pragma once

#include "vulkan/vulkan.h"
#define ALLOC_VMA
#include "vma/vk_mem_alloc.h"
#include <GLFW/glfw3.h>
#include <vector>
#include <string>
#include <array>
#include <optional>
#include <unordered_map>

struct QueueFamilyIndices 
{
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    bool isComplete() const { return graphicsFamily.has_value() && presentFamily.has_value(); }
};

class Device 
{
public:
    // Core Vulkan objects
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice logicalDevice = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VmaAllocator vmaAllocator = VK_NULL_HANDLE;

    // Queues
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;

    // Configuration
    bool enableValidationLayers = false;
    std::vector<const char*> validationLayers;
    std::vector<const char*> instanceExtensions;
    std::vector<const char*> deviceExtensions;
    VkPhysicalDeviceFeatures2 physicalDeviceFeatures{};
    std::vector<VkBaseOutStructure*> extensionFeatureStructs;
    std::unordered_map<std::string, PFN_vkVoidFunction> extensionFunctions;

    // Initialization
    void initInstance(GLFWwindow* window);
    void pickPhysicalDevice(VkSurfaceKHR surface);
    void createLogicalDevice(VkSurfaceKHR surface);
    void createCommandPool();
    void initVMA();

    // Helpers
    void addValidationLayer(const char* layerName);
    void addInstanceExtension(const char* extensionName);
    void addDeviceExtension(const char* extensionName, VkBaseOutStructure* featureStruct = nullptr);
    void registerExtensionFunction(const std::string& functionName);

    template<typename T>
    T getExtensionFunction(const std::string& functionName) const 
    {
        auto it = extensionFunctions.find(functionName);
        if (it != extensionFunctions.end()) {
            return reinterpret_cast<T>(it->second);
        }
        return nullptr;
    }

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);

    ~Device();

private:
    void chainFeatures();
    bool checkValidationLayerSupport();
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface);
};