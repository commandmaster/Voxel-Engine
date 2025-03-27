#pragma once

#define ALLOC_VMA
#include "vma/vk_mem_alloc.h"

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <optional>


namespace VulkanContext
{
	extern VkInstance instance;
    extern VkPhysicalDevice physicalDevice;
    extern VkDevice device;
    extern VkQueue graphicsQueue;
    extern VkQueue presentQueue;
    extern VmaAllocator vmaAllocator;
    extern VkCommandPool commandPool;
    extern VkSurfaceKHR surface;
    
	extern PFN_vkQueueSubmit2KHR vkQueueSubmit2KHR;
	extern PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
	extern PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
	extern PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
	extern PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
	extern PFN_vkBuildAccelerationStructuresKHR vkBuildAccelerationStructuresKHR;
	extern PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;
	extern PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR;
	extern PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR;
	extern PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;
	extern PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;

    extern VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties;
    
    extern const std::vector<const char*> validationLayers;
    extern const std::vector<const char*> deviceExtensions;

	#ifdef NDEBUG
		constexpr bool enableValidationLayers = false;
	#else
		constexpr bool enableValidationLayers = true;
	#endif

	struct SwapChainSupportDetails 
	{
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};
    
    struct QueueFamilyIndices 
    {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;
		inline bool isComplete() const { return graphicsFamily.has_value() && presentFamily.has_value(); }
    };

    void Init(GLFWwindow* window, const char* appName);
    void Cleanup();

    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
	SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);
    
	VkCommandBuffer CreateCommandBuffer(VkCommandBufferLevel level, bool singleUse);
    void SubmitCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free);

}