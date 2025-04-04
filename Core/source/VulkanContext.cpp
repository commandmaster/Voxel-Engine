#include "VulkanContext.h"
#include "DebugUtils.h"

#include <stdexcept>
#include <vector>
#include <string>
#include <unordered_set>
#include <set>

namespace VulkanContext 
{
	class VulkanFeatureChain 
	{
	private:
		void* pNext = nullptr;
		std::vector<void*> features;

	public:
		template<typename T>
		T* addFeature() 
		{
			T* feature = new T{};
			
			feature->sType = getStructureType<T>();
			feature->pNext = nullptr;
			
			if (pNext == nullptr) 
			{
				pNext = feature;
			} 
			else 
			{
				VkBaseOutStructure* last = static_cast<VkBaseOutStructure*>(pNext);
				while (last->pNext != nullptr) 
				{
					last = static_cast<VkBaseOutStructure*>(last->pNext);
				}
				last->pNext = reinterpret_cast<VkBaseOutStructure*>(feature);
			}
			
			features.push_back(feature);
			
			return feature;
		}
		
		template<typename T>
		T* enableFeature(VkBool32 T::*featureFlag = nullptr) 
		{
			T* feature = addFeature<T>();
			if (featureFlag) 
			{
				feature->*featureFlag = VK_TRUE;
			}
			return feature;
		}
		
		void* getChainHead() const 
		{
			return pNext;
		}
		
		void queryFeatures(VkPhysicalDevice device) 
		{
			if (!pNext) return;
			
			VkBaseOutStructure* current = static_cast<VkBaseOutStructure*>(pNext);
			while (current) 
			{
				if (current->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2) 
				{
					vkGetPhysicalDeviceFeatures2(device, reinterpret_cast<VkPhysicalDeviceFeatures2*>(current));
					return;
				}
				current = static_cast<VkBaseOutStructure*>(current->pNext);
			}
			
			auto features2 = addFeature<VkPhysicalDeviceFeatures2>();
			vkGetPhysicalDeviceFeatures2(device, features2);
		}
		
		~VulkanFeatureChain() 
		{
			for (auto feat : features) 
			{
				delete feat;
			}
		}

	private:
		template<typename T>
		VkStructureType getStructureType() 
		{
			if constexpr (std::is_same_v<T, VkPhysicalDeviceFeatures2>) 
			{
				return VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
			} 
			else if constexpr (std::is_same_v<T, VkPhysicalDeviceRayTracingPipelineFeaturesKHR>) 
			{
				return VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
			} 
			else if constexpr (std::is_same_v<T, VkPhysicalDeviceAccelerationStructureFeaturesKHR>) 
			{
				return VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
			} 
			else if constexpr (std::is_same_v<T, VkPhysicalDeviceBufferDeviceAddressFeatures>) 
			{
				return VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
			} 
			else if constexpr (std::is_same_v<T, VkPhysicalDeviceSynchronization2FeaturesKHR>) 
			{
				return VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR;
			} 
			else 
			{
				static_assert(std::is_void_v<T>, "Unsupported feature type");
				return static_cast<VkStructureType>(0);
			}
		}
	};


    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    VmaAllocator vmaAllocator = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;

	PFN_vkQueueSubmit2KHR vkQueueSubmit2KHR = nullptr;
	PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR = nullptr;
	PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR = nullptr;
	PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;
	PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR = nullptr;
	PFN_vkBuildAccelerationStructuresKHR vkBuildAccelerationStructuresKHR = nullptr;
	PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR = nullptr;
	PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR = nullptr;
	PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR = nullptr;
	PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR = nullptr;
	PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR = nullptr;

	VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };


    const std::vector<const char*> validationLayers = 
    {
        "VK_LAYER_KHRONOS_validation",
    };

    const std::vector<const char*> deviceExtensions = 
    {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
		VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
		VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
		VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
		VK_KHR_SPIRV_1_4_EXTENSION_NAME,
		VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
		VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,
		VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME, 
		VK_KHR_BIND_MEMORY_2_EXTENSION_NAME,
		VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
	};

	#pragma region Helpers

	VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback
    (
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData
    ) 
    {
		std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

		return VK_FALSE;
    }

	VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
	{
		auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		if (func != nullptr)
		{
			return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
		}
		else
		{
			return VK_ERROR_EXTENSION_NOT_PRESENT;
		}
	}

	void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
	{
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (func != nullptr)
		{
			func(instance, debugMessenger, pAllocator);
		}
	}


	std::vector<const char*> getRequiredExtensions()
	{
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

		if (enableValidationLayers)
		{
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

		return extensions;
	}

	bool checkValidationLayerSupport()
	{
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layerName : validationLayers)
		{
			bool layerFound = false;

			for (const auto& layerProperties : availableLayers)
			{
				if (strcmp(layerName, layerProperties.layerName) == 0)
				{
					layerFound = true;
					break;
				}
			}

			if (!layerFound)
			{
				return false;
			}
		}


		return true;
	}

	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
	{
		createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = debugCallback;
	}

	    
	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device)
	{
		QueueFamilyIndices indices;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		int i = 0;
		for (const auto& queueFamily : queueFamilies)
		{
			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				indices.graphicsFamily = i;
			}

			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

			if (presentSupport)
			{
				indices.presentFamily = i;
			}

			if (indices.isComplete())
			{
				break;
			}

			i++;
		}

		return indices;

	}

	SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device)
	{
		SwapChainSupportDetails details;

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
		if (formatCount != 0)
		{
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
		}


		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
		if (presentModeCount != 0)
		{
			details.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
		}

		return details;
	}

	VkCommandBuffer CreateCommandBuffer(VkCommandBufferLevel level, bool singleUse, bool autoBegin)
	{
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = level;
        allocInfo.commandPool = commandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;

        VK_ERROR_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer));

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = singleUse ? VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT : 0;

		if (autoBegin)
		{
			VK_ERROR_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));
		}

        return commandBuffer;
	}

	void SubmitCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free)
	{
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        VK_ERROR_CHECK(vkEndCommandBuffer(commandBuffer));

        VkSubmitInfo2 submitInfo2{};
        submitInfo2.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;

        VkCommandBufferSubmitInfo cmdSubmitInfo{};
        cmdSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        cmdSubmitInfo.commandBuffer = commandBuffer;

        submitInfo2.commandBufferInfoCount = 1;
        submitInfo2.pCommandBufferInfos = &cmdSubmitInfo;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

        VkFence fence;
        VK_ERROR_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &fence));

        VK_ERROR_CHECK(VulkanContext::vkQueueSubmit2KHR(queue, 1, &submitInfo2, fence));

        VK_ERROR_CHECK(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));

        if (free)
        {
            vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        }

		vkDestroyFence(device, fence, nullptr);
	}

	
	bool checkDeviceExtensionSupport(VkPhysicalDevice device)
	{
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

		for (const auto& extension : availableExtensions)
		{
			requiredExtensions.erase(extension.extensionName);
		}

		return requiredExtensions.empty();
	}


	bool isDeviceSuitable(VkPhysicalDevice device)
	{
		VkPhysicalDeviceProperties deviceProperties;
		VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceProperties(device, &deviceProperties);
		vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

		QueueFamilyIndices indices = FindQueueFamilies(device);

		bool extensionsSupported = checkDeviceExtensionSupport(device);

		bool swapChainAdequate = false;
		if (extensionsSupported)
		{
			SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(device);
			swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
		}

		VulkanFeatureChain queryChain;
		
		auto sync2Features = queryChain.enableFeature<VkPhysicalDeviceSynchronization2FeaturesKHR>(&VkPhysicalDeviceSynchronization2FeaturesKHR::synchronization2);
		auto bufferDeviceAddressFeatures = queryChain.enableFeature<VkPhysicalDeviceBufferDeviceAddressFeatures>(&VkPhysicalDeviceBufferDeviceAddressFeatures::bufferDeviceAddress);
		auto accStructFeatures = queryChain.enableFeature<VkPhysicalDeviceAccelerationStructureFeaturesKHR>(&VkPhysicalDeviceAccelerationStructureFeaturesKHR::accelerationStructure);
		auto rtPipelineFeatures = queryChain.enableFeature<VkPhysicalDeviceRayTracingPipelineFeaturesKHR>(&VkPhysicalDeviceRayTracingPipelineFeaturesKHR::rayTracingPipeline);
		
		queryChain.queryFeatures(device);

		bool hasRequiredDeviceFeatures =
			deviceFeatures.geometryShader &&
			bufferDeviceAddressFeatures->bufferDeviceAddress &&
			accStructFeatures->accelerationStructure &&
			rtPipelineFeatures->rayTracingPipeline &&
			sync2Features->synchronization2;

		return deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && hasRequiredDeviceFeatures && indices.isComplete() && extensionsSupported && swapChainAdequate;
	}
    
    
	#pragma endregion

    
	void setupDebugMessenger()
	{
		if (!enableValidationLayers) return;

		VkDebugUtilsMessengerCreateInfoEXT createInfo;
		populateDebugMessengerCreateInfo(createInfo);

		if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS)
		{
			LOG_ERROR("failed to set up debug messenger!");
		}
	}

    void createInstance(const char* appName)
    {
		if (enableValidationLayers && !checkValidationLayerSupport())
		{
			LOG_ERROR("validation layers requested, but not available!");
		}

		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = appName;
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
		if (enableValidationLayers)
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();

			populateDebugMessengerCreateInfo(debugCreateInfo);
			debugCreateInfo.messageSeverity = 
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
		}
		else
		{
			createInfo.enabledLayerCount = 0;
			createInfo.pNext = nullptr;
		}

		auto extensions = getRequiredExtensions();
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();

		if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
		{
			LOG_ERROR("failed to create instance!");
		}
    }

	void createSurface(GLFWwindow* window)
	{
		if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
		{
			LOG_ERROR("failed to create window surface!");
		}
	}

	void pickPhysicalDevice()
	{
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

		if (deviceCount == 0)
		{
			LOG_ERROR("failed to find GPUs with Vulkan support!");
		}


		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

		for (const auto& device : devices)
		{
			if (isDeviceSuitable(device))
			{
				physicalDevice = device;
				break;
			}
		}

		if (physicalDevice == VK_NULL_HANDLE)
		{
			LOG_ERROR("failed to find a suitable GPU!");
		}

		
		VkPhysicalDeviceMaintenance3Properties maintenanceProperties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES };
		VkPhysicalDeviceProperties2 prop2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
		prop2.pNext = &maintenanceProperties;

		vkGetPhysicalDeviceProperties2(physicalDevice, &prop2);


		LOG_NORMAL("Max Memory Allocation Size: " + std::to_string(maintenanceProperties.maxMemoryAllocationSize));
	}

	void createLogicalDevice()
	{
		QueueFamilyIndices indices = FindQueueFamilies(physicalDevice);

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

		float queuePriority = 1.0f;

		for (uint32_t queueFamily : uniqueQueueFamilies)
		{
			VkDeviceQueueCreateInfo queueCreateInfo{};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}

        rtProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
        VkPhysicalDeviceProperties2 prop2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
        prop2.pNext = &rtProperties;
		vkGetPhysicalDeviceProperties2(physicalDevice, &prop2);

		
		VulkanFeatureChain featureChain;

		auto rtPipelineFeatures = featureChain.enableFeature<VkPhysicalDeviceRayTracingPipelineFeaturesKHR>(&VkPhysicalDeviceRayTracingPipelineFeaturesKHR::rayTracingPipeline);
		
		auto accelerationStructureFeatures = featureChain.enableFeature<VkPhysicalDeviceAccelerationStructureFeaturesKHR>(&VkPhysicalDeviceAccelerationStructureFeaturesKHR::accelerationStructure);
		
		auto bufferDeviceAddressFeatures = featureChain.enableFeature<VkPhysicalDeviceBufferDeviceAddressFeatures>(&VkPhysicalDeviceBufferDeviceAddressFeatures::bufferDeviceAddress);
		
		auto sync2Features = featureChain.enableFeature<VkPhysicalDeviceSynchronization2FeaturesKHR>(&VkPhysicalDeviceSynchronization2FeaturesKHR::synchronization2);
		
		auto deviceExtensionFeatures = featureChain.addFeature<VkPhysicalDeviceFeatures2>();

		VkDeviceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.pNext = featureChain.getChainHead();
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
		createInfo.ppEnabledExtensionNames = deviceExtensions.data();

		if (enableValidationLayers)
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else
		{
			createInfo.enabledLayerCount = 0;
		}
		
		if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS)
		{
			LOG_ERROR("failed to create logical device!");
		}

		vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
		vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);

		vkQueueSubmit2KHR = 
			reinterpret_cast<PFN_vkQueueSubmit2KHR>(
				vkGetDeviceProcAddr(device, "vkQueueSubmit2KHR")
			);

		vkGetAccelerationStructureBuildSizesKHR = 
			reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(
				vkGetDeviceProcAddr(device, "vkGetAccelerationStructureBuildSizesKHR")
			);

		vkCreateAccelerationStructureKHR = 
			reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(
				vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR")
			);

		vkGetAccelerationStructureDeviceAddressKHR = 
			reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(
				vkGetDeviceProcAddr(device, "vkGetAccelerationStructureDeviceAddressKHR")
			);

		vkCmdBuildAccelerationStructuresKHR = 
			reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(
				vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR")
			);

		vkBuildAccelerationStructuresKHR = 
			reinterpret_cast<PFN_vkBuildAccelerationStructuresKHR>(
				vkGetDeviceProcAddr(device, "vkBuildAccelerationStructuresKHR")
			);

		vkDestroyAccelerationStructureKHR = 
			reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(
				vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR")
			);

		vkGetRayTracingShaderGroupHandlesKHR = 
			reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(
				vkGetDeviceProcAddr(device, "vkGetRayTracingShaderGroupHandlesKHR")
			);

		vkGetBufferDeviceAddressKHR = 
			reinterpret_cast<PFN_vkGetBufferDeviceAddressKHR>(
				vkGetDeviceProcAddr(device, "vkGetBufferDeviceAddressKHR")
			);

		vkCreateRayTracingPipelinesKHR = 
			reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(
				vkGetDeviceProcAddr(device, "vkCreateRayTracingPipelinesKHR")
			);

		vkCmdTraceRaysKHR =
			reinterpret_cast<PFN_vkCmdTraceRaysKHR>(
				vkGetDeviceProcAddr(device, "vkCmdTraceRaysKHR")
			);


	}

	void createCommandPool()
	{
		QueueFamilyIndices queueFamilyIndices = FindQueueFamilies(physicalDevice);

		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

		if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
		{
			LOG_ERROR("failed to create command pool!");
		}
	}

	void Init(GLFWwindow* wnd, const char* appName) 
    {
		createInstance(appName);
		setupDebugMessenger();
		createSurface(wnd);
		pickPhysicalDevice();
		createLogicalDevice();

		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

		for (uint32_t i = 0; i < memProperties.memoryHeapCount; i++) {
			std::cout << "Heap " << i << " size: " 
					  << (memProperties.memoryHeaps[i].size / (1024.0 * 1024.0)) 
					  << " MB" << std::endl;
		}


        VmaAllocatorCreateInfo allocatorInfo = {};
        allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_2;
        allocatorInfo.physicalDevice = physicalDevice;
        allocatorInfo.device = device;
        allocatorInfo.instance = instance;
		allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
		vmaCreateAllocator(&allocatorInfo, &vmaAllocator);

		createCommandPool();
    }

    void Cleanup() 
    {
		vkDestroyCommandPool(device, commandPool, nullptr);
		vmaDestroyAllocator(vmaAllocator);
		
		if (enableValidationLayers)
		{
			DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
		}

		vkDestroyDevice(device, nullptr);
		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);
    }
}

