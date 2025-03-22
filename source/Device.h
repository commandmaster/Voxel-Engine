#pragma once

#include "vulkan/vulkan.h"

#define ALLOC_VMA
#include "vma/vk_mem_alloc.h"

#include <vector>
#include <string>
#include <array>

struct Device
{
public:
	VkInstance instance = VK_NULL_HANDLE; 

	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice logicalDevice = VK_NULL_HANDLE;
	
	VkCommandPool commandPool = VK_NULL_HANDLE;

	std::vector<const char*> requiredExtensions;
	std::vector<VkBaseOutStructure*> extensionFeatureStructs;
	VkPhysicalDeviceFeatures2 physicalDeviceFeatures;

	void addDeviceExtension(const char* extensionName, bool required = true, VkBaseOutStructure* featureStruct = nullptr) 
	{
        requiredExtensions.push_back(extensionName);
        if (featureStruct) 
		{
            extensionFeatureStructs.push_back(featureStruct);
        }
    }

	void init()
	{
		
	}

	~Device()
	{
		if (commandPool != VK_NULL_HANDLE)
		{
			vkDestroyCommandPool(device, commandPool, nullptr);
		}

		if (logicalDevice != VK_NULL_HANDLE)
		{
			vkDestroyDevice(logicalDevice, nullptr);
		}

		if (instance != VK_NULL_HANDLE)
		{
			vkDestroyInstance(instance, nullptr);
		}
	}

private:	
	void chainFeatures()
	{
		VkBaseOutStructure* last = reinterpret_cast<VkBaseOutStructure*>(&physicalDeviceFeatures);
        for (auto* feature : extensionFeatureStructs) 
		{
            last->pNext = feature;
            last = feature;
        }
	}

	bool checkPhysicalDeviceExtensionSupport()
	{
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

		for (const auto& ext : availableExtensions) {
			if (strcmp(ext.extensionName, extensionName) == 0) {
				return true;
			}
		}
		return false;
	}

	bool isDeviceSuitable()
	{

	}
};

