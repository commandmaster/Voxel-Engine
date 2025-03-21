#include "VoxelEngine.hpp"

#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"

std::string Shader::readShaderFile(const std::string& path)
{
	std::ifstream file(path);
	if (!file.is_open())
	{
		throw std::runtime_error("Failed to open shader file: " + path);
	}

	std::stringstream buffer;
	buffer << file.rdbuf();
	return buffer.str();
}

std::vector<uint32_t> Shader::compileGLSLToSPIRV(const std::string& source, shaderc_shader_kind kind, const char* shaderName)
{
	shaderc::Compiler compiler;
	shaderc::CompileOptions options;

	options.SetOptimizationLevel(shaderc_optimization_level_performance);
	options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
	options.SetTargetSpirv(shaderc_spirv_version_1_4);

	// Compile the GLSL source to SPIR-V
	shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(source, kind, shaderName, options);

	if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
		std::cerr << "Shader compilation failed: " << module.GetErrorMessage() << std::endl;
		return {};
	}

	// Copy the SPIR-V result into a vector
	return { module.cbegin(), module.cend() };
}

VkShaderModule Shader::createShaderModule(VkDevice device, const std::vector<uint32_t>& spirv)
{
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = spirv.size() * sizeof(uint32_t);
	createInfo.pCode = reinterpret_cast<const uint32_t*>(spirv.data());

	VkShaderModule shaderModule;
	if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create shader module!");
	}

	return shaderModule;
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

PFN_vkQueueSubmit2KHR VoxelEngine::vkQueueSubmit2KHR = nullptr;
PFN_vkGetAccelerationStructureBuildSizesKHR VoxelEngine::vkGetAccelerationStructureBuildSizesKHR = nullptr;
PFN_vkCreateAccelerationStructureKHR VoxelEngine::vkCreateAccelerationStructureKHR = nullptr;
PFN_vkGetAccelerationStructureDeviceAddressKHR VoxelEngine::vkGetAccelerationStructureDeviceAddressKHR = nullptr;
PFN_vkCmdBuildAccelerationStructuresKHR VoxelEngine::vkCmdBuildAccelerationStructuresKHR = nullptr;
PFN_vkDestroyAccelerationStructureKHR VoxelEngine::vkDestroyAccelerationStructureKHR = nullptr;
PFN_vkGetRayTracingShaderGroupHandlesKHR VoxelEngine::vkGetRayTracingShaderGroupHandlesKHR = nullptr;
PFN_vkGetBufferDeviceAddressKHR VoxelEngine::vkGetBufferDeviceAddressKHR = nullptr;
PFN_vkCreateRayTracingPipelinesKHR VoxelEngine::vkCreateRayTracingPipelinesKHR = nullptr;
PFN_vkCmdTraceRaysKHR VoxelEngine::vkCmdTraceRaysKHR = nullptr;


void VoxelEngine::mouseCallback(GLFWwindow* window, double xpos, double ypos)
{
	VoxelEngine* app = static_cast<VoxelEngine*>(glfwGetWindowUserPointer(window));
    
    if (app->firstMouse) {
        app->lastMouseX = xpos;
        app->lastMouseY = ypos;
        app->firstMouse = false;
    }

    float xoffset = xpos - app->lastMouseX;
    float yoffset = app->lastMouseY - ypos; 
    app->lastMouseX = xpos;
    app->lastMouseY = ypos;

    app->camera.rotate(xoffset, yoffset);
}

void VoxelEngine::initWindow()
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);


	window = glfwCreateWindow(WIDTH, HEIGHT, windowName.c_str(), nullptr, nullptr);

	glfwSetCursorPosCallback(window, VoxelEngine::mouseCallback);

	glfwSetWindowUserPointer(window, this);
	glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
}

 std::vector<const char*> VoxelEngine::getRequiredExtensions()
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

 bool VoxelEngine::checkValidationLayerSupport()
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

void VoxelEngine::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
	createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = debugCallback;
}

void VoxelEngine::setupDebugMessenger()
{
	if (!enableValidationLayers) return;

	VkDebugUtilsMessengerCreateInfoEXT createInfo;
	populateDebugMessengerCreateInfo(createInfo);

	if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to set up debug messenger!");
	}
}

void VoxelEngine::createInstance()
{
	if (enableValidationLayers && !checkValidationLayerSupport())
	{
		throw std::runtime_error("validation layers requested, but not available!");
	}

	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = windowName.c_str();
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
		throw std::runtime_error("failed to create instance!");
	}
}

 QueueFamilyIndices VoxelEngine::findQueueFamilies(VkPhysicalDevice device)
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

 bool VoxelEngine::checkDeviceExtensionSupport(VkPhysicalDevice device)
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

 SwapChainSupportDetails VoxelEngine::querySwapChainSupport(VkPhysicalDevice device)
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

bool VoxelEngine::isDeviceSuitable(VkPhysicalDevice device)
{
	VkPhysicalDeviceProperties deviceProperties;
	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

	QueueFamilyIndices indices = findQueueFamilies(device);

	bool extensionsSupported = checkDeviceExtensionSupport(device);

	bool swapChainAdequate = false;
	if (extensionsSupported)
	{
		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
		swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
	}

	VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures{};
    rayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures{};
    accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    accelerationStructureFeatures.pNext = &rayTracingPipelineFeatures;

    VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures{};
    bufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
    bufferDeviceAddressFeatures.pNext = &accelerationStructureFeatures;

	VkPhysicalDeviceSynchronization2FeaturesKHR synchronization2Features{};
	synchronization2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR;
	synchronization2Features.pNext = &bufferDeviceAddressFeatures;


    VkPhysicalDeviceFeatures2 physicalFeatures2{};
    physicalFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    physicalFeatures2.pNext = &synchronization2Features;

    vkGetPhysicalDeviceFeatures2(device, &physicalFeatures2);

	bool hasRequiredDeviceFeatures =
		deviceFeatures.geometryShader &&
		bufferDeviceAddressFeatures.bufferDeviceAddress &&
		accelerationStructureFeatures.accelerationStructure &&
		rayTracingPipelineFeatures.rayTracingPipeline &&
		synchronization2Features.synchronization2;

	return deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && hasRequiredDeviceFeatures && indices.isComplete() && extensionsSupported && swapChainAdequate;
}

 VkSurfaceFormatKHR VoxelEngine::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
	for (const auto& availableFormat : availableFormats)
	{
		if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return availableFormat;
		}
	}

	return availableFormats[0];
}

 VkPresentModeKHR VoxelEngine::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
{
	for (const auto& availablePresentMode : availablePresentModes) {
		if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
			return availablePresentMode;
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

 VkExtent2D VoxelEngine::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
	{
		return capabilities.currentExtent;
	}
	else
	{
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);

		VkExtent2D actualExtent = {
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height)
		};

		actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

		return actualExtent;
	}
}

void VoxelEngine::pickPhysicalDevice()
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

	if (deviceCount == 0)
	{
		throw std::runtime_error("failed to find GPUs with Vulkan support!");
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
		throw std::runtime_error("failed to find a suitable GPU!");
	}
	
	VkPhysicalDeviceProperties2 prop2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
	prop2.pNext = &rtProperties;
	vkGetPhysicalDeviceProperties2(physicalDevice, &prop2);

}


void VoxelEngine::createLogicalDevice()
{
	QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

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

	VkPhysicalDeviceFeatures deviceFeatures{};

	VkPhysicalDeviceFeatures2 deviceExtensionFeatures{};
	deviceExtensionFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

	VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures{};
    rayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
	rayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;

	accelerationStructureFeatures.accelerationStructure = VK_TRUE;
    accelerationStructureFeatures.pNext = &rayTracingPipelineFeatures;

	VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures{};
    bufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
	bufferDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE;
    bufferDeviceAddressFeatures.pNext = &accelerationStructureFeatures;

	VkPhysicalDeviceSynchronization2FeaturesKHR synchronization2Features{};
	synchronization2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR;
	synchronization2Features.synchronization2 = VK_TRUE;

	synchronization2Features.pNext = &bufferDeviceAddressFeatures;

	deviceExtensionFeatures.pNext = &synchronization2Features;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.pNext = &deviceExtensionFeatures;
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
		throw std::runtime_error("failed to create logical device!");
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

void VoxelEngine::createSurface()
{
	if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create window surface!");
	}
}

void VoxelEngine::createSwapChain()
{
	SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

	VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
	VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
	VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

	uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
	if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
	{
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = surface;

	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
	uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

	if (indices.graphicsFamily != indices.presentFamily)
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0;
		createInfo.pQueueFamilyIndices = nullptr;
	}

	createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // Blending with other windows - Should be left opaque

	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;

	createInfo.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create swap chain!");
	}

	vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr); // we specify the minimum number of images but the implementation can create more, thus here we check the actual image count
	swapChainImages.resize(imageCount); // Resize to match correct image count
	vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data()); // now we can specify the data as the vector is the correct size

	swapChainImageFormat = surfaceFormat.format;
	swapChainExtent = extent;
}

void VoxelEngine::createImageViews()
{
	swapChainImageViews.resize(swapChainImages.size());

	for (size_t i = 0; i < swapChainImages.size(); ++i)
	{
		VkImageViewCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = swapChainImages[i];

		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = swapChainImageFormat; // (color format) <- later when you forget what the format 'means' :)

		// default color mappings
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create image views!");
		}
	}
}



void VoxelEngine::createCommandPool()
{
	QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

	if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create command pool!");
	}
}

VkCommandBuffer VoxelEngine::createCommandBuffer(VkDevice device, VkCommandPool commandPool, VkCommandBufferLevel level, bool singleUse)
{
	VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = level;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS) 
	{
        throw std::runtime_error("Failed to allocate command buffer");
    }

    if (singleUse) 
	{
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(commandBuffer, &beginInfo);
    }

    return commandBuffer;	
}

void VoxelEngine::flushCommandBuffer(VkDevice device, VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool commandPool, bool free)
{
	if (commandBuffer == VK_NULL_HANDLE) return;

    VkCommandBufferSubmitInfoKHR submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO_KHR;
    submitInfo.commandBuffer = commandBuffer;
    
    VkFence fence;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    vkCreateFence(device, &fenceInfo, nullptr, &fence);

    VkSubmitInfo2KHR finalSubmit{};
    finalSubmit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR;
    finalSubmit.commandBufferInfoCount = 1;
    finalSubmit.pCommandBufferInfos = &submitInfo;
    
    vkEndCommandBuffer(commandBuffer);
    vkQueueSubmit2KHR(queue, 1, &finalSubmit, fence);

    vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
    vkDestroyFence(device, fence, nullptr);

    if (free)
	{
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    }
}


void VoxelEngine::createCommandBuffers()
{
	commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

	VkCommandBufferAllocateInfo allocInfo{};

	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

	if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate command buffers!");
	}

}


void VoxelEngine::initVMA()
{
	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_2;
	allocatorInfo.physicalDevice = physicalDevice;
	allocatorInfo.device = device;
	allocatorInfo.instance = instance;
	allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT; 
	vmaCreateAllocator(&allocatorInfo, &vmaAllocator);
}

uint64_t VoxelEngine::getBufferDeviceAddress(VkBuffer buffer)
{
	VkBufferDeviceAddressInfoKHR bufferDeviceAddressInfo{};
	bufferDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	bufferDeviceAddressInfo.buffer = buffer;
	
	return vkGetBufferDeviceAddressKHR(device, &bufferDeviceAddressInfo);
}

void VoxelEngine::createStorageImages()
{
    storageImages.resize(MAX_FRAMES_IN_FLIGHT);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        storageImages[i].destroy();
        VkExtent3D extent;
        extent.depth = 1;
        extent.width = swapChainExtent.width;
        extent.height = swapChainExtent.height;
        storageImages[i].create(vmaAllocator, device, extent, 
            VK_FORMAT_B8G8R8A8_UNORM, 
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT);

        // Transition layout from UNDEFINED to GENERAL
        VkCommandBuffer cmd = createCommandBuffer(device, commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
        
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = storageImages[i].image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );

        flushCommandBuffer(device, cmd, graphicsQueue, commandPool, true);
    }
}

void VoxelEngine::createBLAS()
{
    struct VkAabbPositionsKHR
    {
        float minX, minY, minZ;
        float maxX, maxY, maxZ;
    };

    std::vector<VkAabbPositionsKHR> aabbs{};
    aabbs.reserve(spheres.size());

    for (const auto& sphere : spheres)
    {
        // w component is radius
        aabbs.push_back({
            sphere.positionRadius.x - sphere.positionRadius.w,
            sphere.positionRadius.y - sphere.positionRadius.w,
            sphere.positionRadius.z - sphere.positionRadius.w,
            sphere.positionRadius.x + sphere.positionRadius.w,
            sphere.positionRadius.y + sphere.positionRadius.w,
            sphere.positionRadius.z + sphere.positionRadius.w,
        });
    }

    VkDeviceSize aabbBufferSize = aabbs.size() * sizeof(VkAabbPositionsKHR);

    const VkBufferUsageFlags bufferUsageFlags = 
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | 
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    ManagedBuffer aabbBuffer;
    aabbBuffer.create(vmaAllocator, device, aabbBufferSize, 
                     bufferUsageFlags, 
                     ManagedBuffer::BufferType::HostVisible);
    aabbBuffer.updateData(vmaAllocator, aabbs.data(), aabbBufferSize);

    VkDeviceOrHostAddressConstKHR aabbDataDeviceAddress{};
    aabbDataDeviceAddress.deviceAddress = getBufferDeviceAddress(aabbBuffer.handle);

    VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
    accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_AABBS_KHR;
    accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    
    accelerationStructureGeometry.geometry.aabbs.sType = 
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR;
    accelerationStructureGeometry.geometry.aabbs.data = aabbDataDeviceAddress;
    accelerationStructureGeometry.geometry.aabbs.stride = sizeof(VkAabbPositionsKHR);

    VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
    accelerationStructureBuildGeometryInfo.sType = 
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    accelerationStructureBuildGeometryInfo.type = 
        VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    accelerationStructureBuildGeometryInfo.flags = 
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    accelerationStructureBuildGeometryInfo.geometryCount = 1;
    accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

    const uint32_t primitiveCount = static_cast<uint32_t>(aabbs.size());

    VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
    accelerationStructureBuildSizesInfo.sType = 
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    vkGetAccelerationStructureBuildSizesKHR(
        device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &accelerationStructureBuildGeometryInfo,
        &primitiveCount,
        &accelerationStructureBuildSizesInfo);

    ManagedBuffer accelerationStorage;
    accelerationStorage.create(
        vmaAllocator,
        device,
        accelerationStructureBuildSizesInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | 
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        ManagedBuffer::BufferType::DeviceLocal
    );

    VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
    accelerationStructureCreateInfo.sType = 
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    accelerationStructureCreateInfo.buffer = accelerationStorage.handle;
    accelerationStructureCreateInfo.size = 
        accelerationStructureBuildSizesInfo.accelerationStructureSize;
    accelerationStructureCreateInfo.type = 
        VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    
    VkAccelerationStructureKHR blasHandle;
    vkCreateAccelerationStructureKHR(
        device,
        &accelerationStructureCreateInfo,
        nullptr,
        &blasHandle);

    ScratchBuffer scratchBuffer;
    ScratchBuffer::createScratchBuffer(vmaAllocator, device, 
        accelerationStructureBuildSizesInfo.buildScratchSize, scratchBuffer);

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.dstAccelerationStructure = blasHandle;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &accelerationStructureGeometry;
    buildInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

    VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
    buildRangeInfo.primitiveCount = primitiveCount;
    buildRangeInfo.primitiveOffset = 0;
    buildRangeInfo.firstVertex = 0;
    buildRangeInfo.transformOffset = 0;

    std::vector<VkAccelerationStructureBuildRangeInfoKHR*> buildRangeInfos = {&buildRangeInfo};

    VkCommandBuffer cmd = createCommandBuffer(device, commandPool, 
        VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, buildRangeInfos.data());
    flushCommandBuffer(device, cmd, graphicsQueue, commandPool, true);

    ScratchBuffer::destroyScratchBuffer(vmaAllocator, scratchBuffer);
    aabbBuffer.destroy(vmaAllocator);

    bottomLevelAccelerationStructure.handle = blasHandle;
    bottomLevelAccelerationStructure.buffer = std::move(accelerationStorage);

    VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
    addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    addressInfo.accelerationStructure = blasHandle;
    bottomLevelAccelerationStructure.deviceAddress = 
        vkGetAccelerationStructureDeviceAddressKHR(device, &addressInfo);
}


void VoxelEngine::createTLAS()
{
	VkTransformMatrixKHR transformMatrix = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f};

    VkAccelerationStructureInstanceKHR instance{};
    instance.transform = transformMatrix;
    instance.instanceCustomIndex = 0;
    instance.mask = 0xFF;
    instance.instanceShaderBindingTableRecordOffset = 0;
    instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    instance.accelerationStructureReference = bottomLevelAccelerationStructure.deviceAddress;

    // Create instance buffer
    ManagedBuffer instancesBuffer;
    const VkDeviceSize instanceBufferSize = sizeof(instance);
    instancesBuffer.create(vmaAllocator, device, instanceBufferSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        ManagedBuffer::BufferType::HostVisible);
    instancesBuffer.updateData(vmaAllocator, &instance, instanceBufferSize);

    // Get device address of instance buffer
    VkDeviceOrHostAddressConstKHR instanceDataAddress{};
    instanceDataAddress.deviceAddress = getBufferDeviceAddress(instancesBuffer.handle);

    // Set up TLAS geometry
    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    geometry.geometry.instances.arrayOfPointers = VK_FALSE;
    geometry.geometry.instances.data = instanceDataAddress;

    // Get build sizes
    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geometry;

    const uint32_t primitiveCount = 1;
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    vkGetAccelerationStructureBuildSizesKHR(
        device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo,
        &primitiveCount,
        &sizeInfo);

    // Create TLAS buffer
	topLevelAccelerationStructure.buffer.create(
		vmaAllocator,
		device,
		sizeInfo.accelerationStructureSize,
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		ManagedBuffer::BufferType::DeviceLocal
	);

    ManagedBuffer& accelerationStorage = topLevelAccelerationStructure.buffer;

    // Create acceleration structure
    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = accelerationStorage.handle;
    createInfo.size = sizeInfo.accelerationStructureSize;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

    VkAccelerationStructureKHR tlasHandle;
    if (vkCreateAccelerationStructureKHR(device, &createInfo, nullptr, &tlasHandle) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create TLAS");
    }

    // Create scratch buffer
    ScratchBuffer scratchBuffer;
    ScratchBuffer::createScratchBuffer(vmaAllocator, device, sizeInfo.buildScratchSize, scratchBuffer);

    // Build TLAS
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.dstAccelerationStructure = tlasHandle;
    buildInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

    VkAccelerationStructureBuildRangeInfoKHR buildRange{};
    buildRange.primitiveCount = primitiveCount;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR*> buildRanges = {&buildRange};

    VkCommandBuffer cmd = createCommandBuffer(device, commandPool, 
        VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, buildRanges.data());
    flushCommandBuffer(device, cmd, graphicsQueue, commandPool, true);

    // Cleanup
    ScratchBuffer::destroyScratchBuffer(vmaAllocator, scratchBuffer);
    instancesBuffer.destroy(vmaAllocator);

    // Store results
    topLevelAccelerationStructure.handle = tlasHandle;

    // Get device address
    VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
    addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    addressInfo.accelerationStructure = tlasHandle;
    topLevelAccelerationStructure.deviceAddress = 
        vkGetAccelerationStructureDeviceAddressKHR(device, &addressInfo);
}

void VoxelEngine::deleteAS(AccelerationStructure& accelerationStructure)
{
	if (accelerationStructure.handle != VK_NULL_HANDLE) {
        vkDestroyAccelerationStructureKHR(device, accelerationStructure.handle, nullptr);
        accelerationStructure.handle = VK_NULL_HANDLE;
    }
    accelerationStructure.buffer.destroy(vmaAllocator);
}

void VoxelEngine::createShaderBindingTables() 
{
    const uint32_t handleSize = rtProperties.shaderGroupHandleSize;
    const uint32_t handleSizeAligned = alignedSize(handleSize, rtProperties.shaderGroupBaseAlignment);
    const uint32_t groupCount = static_cast<uint32_t>(shaderGroups.size());
    
    // Create SBT buffers with size = handleSizeAligned (aligned to GPU requirements)
    raygenShaderBindingTable.create(vmaAllocator, device, handleSizeAligned, 
        VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        ManagedBuffer::BufferType::DeviceLocal);
    missShaderBindingTable.create(vmaAllocator, device, handleSizeAligned, 
        VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        ManagedBuffer::BufferType::DeviceLocal);
    closestHitShaderBindingTable.create(vmaAllocator, device, handleSizeAligned, 
        VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        ManagedBuffer::BufferType::DeviceLocal);

    // Staging buffer must hold all groups with aligned entries
    const VkDeviceSize stagingSize = groupCount * handleSizeAligned;
    ManagedBuffer stagingBuffer;
    stagingBuffer.create(vmaAllocator, device, stagingSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        ManagedBuffer::BufferType::HostVisible);

    // Get shader handles into staging buffer (aligned)
    std::vector<uint8_t> shaderHandleStorage(stagingSize);
    vkGetRayTracingShaderGroupHandlesKHR(
        device, rtPipeline, 0, groupCount, stagingSize, shaderHandleStorage.data()
    );
    stagingBuffer.updateData(vmaAllocator, shaderHandleStorage.data(), stagingSize);

    // Copy aligned chunks from staging to each SBT buffer (dstOffset = 0)
    VkCommandBuffer cmd = createCommandBuffer(device, commandPool, 
        VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    
    VkBufferCopy copyRegion{};
    copyRegion.size = handleSizeAligned;

    // Raygen (group 0)
    copyRegion.srcOffset = 0 * handleSizeAligned;
    vkCmdCopyBuffer(cmd, stagingBuffer.handle, raygenShaderBindingTable.handle, 1, &copyRegion);

    // Miss (group 1)
    copyRegion.srcOffset = 1 * handleSizeAligned;
    vkCmdCopyBuffer(cmd, stagingBuffer.handle, missShaderBindingTable.handle, 1, &copyRegion);

    // Hit (group 2)
    copyRegion.srcOffset = 2 * handleSizeAligned;
    vkCmdCopyBuffer(cmd, stagingBuffer.handle, closestHitShaderBindingTable.handle, 1, &copyRegion);

    flushCommandBuffer(device, cmd, graphicsQueue, commandPool, true);
    stagingBuffer.destroy(vmaAllocator);
}

void VoxelEngine::createDescriptorSetsRT()
{
	std::array<VkDescriptorPoolSize, 4> poolSizes = {
		{
			{VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, MAX_FRAMES_IN_FLIGHT},
			{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, MAX_FRAMES_IN_FLIGHT},
			{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT},
			{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_FRAMES_IN_FLIGHT}
		}
	};

	VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = MAX_FRAMES_IN_FLIGHT,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()), // Use correct count
        .pPoolSizes = poolSizes.data() // Use fixed array
    };

	if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
	{
		throw std::runtime_error("could not create descriptor pool");
	}

	std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayoutRT);
	VkDescriptorSetAllocateInfo allocInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = descriptorPool,
		.descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
		.pSetLayouts = layouts.data()
	};

	descriptorSetsRT.resize(MAX_FRAMES_IN_FLIGHT);
	if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSetsRT.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("could not allocate descriptor set");
	}

	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		std::array<VkWriteDescriptorSet, 4> descriptorWrites = {};

		// Acceleration structure (Binding 0)
		VkWriteDescriptorSetAccelerationStructureKHR asInfo = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
			.accelerationStructureCount = 1,
			.pAccelerationStructures = &topLevelAccelerationStructure.handle  
		};

		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = descriptorSetsRT[i];
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
		descriptorWrites[0].pNext = &asInfo;

		// Storage image (Binding 1)
		VkDescriptorImageInfo imageInfo = {
			.imageView = storageImages[i].view,
			.imageLayout = VK_IMAGE_LAYOUT_GENERAL  
		};

		descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[1].dstSet = descriptorSetsRT[i];
		descriptorWrites[1].dstBinding = 1;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		descriptorWrites[1].pImageInfo = &imageInfo;

		// Storage buffer (Binding 2)
		VkDescriptorBufferInfo bufferInfo = {
			.buffer = ubo.handle,
			.offset = 0,
			.range = VK_WHOLE_SIZE
		};

		descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[2].dstSet = descriptorSetsRT[i];
		descriptorWrites[2].dstBinding = 2;
		descriptorWrites[2].descriptorCount = 1;
		descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[2].pBufferInfo = &bufferInfo;

		VkDescriptorBufferInfo sphereBufferInfo{
			.buffer = sphereBuffer.handle,
			.offset = 0,
			.range = VK_WHOLE_SIZE
		};

		descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[3].dstSet = descriptorSetsRT[i];
		descriptorWrites[3].dstBinding = 3;
		descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorWrites[3].descriptorCount = 1;
		descriptorWrites[3].pBufferInfo = &sphereBufferInfo;

		vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	}

}

void VoxelEngine::updateDescriptorSetsRT()
{
	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) 
	{
		VkDescriptorImageInfo imageInfo{
			.imageView = storageImages[i].view,
			.imageLayout = VK_IMAGE_LAYOUT_GENERAL
		};

		VkWriteDescriptorSet write{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = descriptorSetsRT[i],
			.dstBinding = 1, 
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			.pImageInfo = &imageInfo
		};

		vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

	}
}

void VoxelEngine::createRayTracingPipeline()
{
	VkDescriptorSetLayoutBinding accelerationStructureLayoutBinding{};
	accelerationStructureLayoutBinding.binding = 0;
	accelerationStructureLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	accelerationStructureLayoutBinding.descriptorCount = 1;
	accelerationStructureLayoutBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

	VkDescriptorSetLayoutBinding resultImageLayoutBinding{};
	resultImageLayoutBinding.binding         = 1;
	resultImageLayoutBinding.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	resultImageLayoutBinding.descriptorCount = 1;
	resultImageLayoutBinding.stageFlags      = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

	VkDescriptorSetLayoutBinding uniformBufferBinding{};
    uniformBufferBinding.binding         = 2;
    uniformBufferBinding.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uniformBufferBinding.descriptorCount = 1;
    uniformBufferBinding.stageFlags      = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

	VkDescriptorSetLayoutBinding sphereBufferBinding{};
	sphereBufferBinding.binding = 3;
	sphereBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	sphereBufferBinding.descriptorCount = 1;
	sphereBufferBinding.stageFlags = VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	

	std::vector<VkDescriptorSetLayoutBinding> bindings = {
	    accelerationStructureLayoutBinding,
	    resultImageLayoutBinding,
		uniformBufferBinding,
		sphereBufferBinding
	};


	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings    = bindings.data();

	if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayoutRT) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create descriptor set layout");
	}

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayoutRT;

	if (vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &rtPipelineLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create pipeline layout (RT)");
	}

	std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

	auto raygenShaderGLSL = Shader::readShaderFile("C:/Users/Bennett/source/repos/VoxelEngine/shaders/shader.rgen");
	auto missShaderGLSL = Shader::readShaderFile("C:/Users/Bennett/source/repos/VoxelEngine/shaders/shader.rmiss");
	auto closestHitShaderGLSL = Shader::readShaderFile("C:/Users/Bennett/source/repos/VoxelEngine/shaders/shader.rchit");
	auto intersectionShaderGLSL = Shader::readShaderFile("C:/Users/Bennett/source/repos/VoxelEngine/shaders/shader.rint");

	auto raygenShaderSPIRV = Shader::compileGLSLToSPIRV(raygenShaderGLSL, shaderc_raygen_shader, "Raygen Shader");
	auto missShaderSPIRV = Shader::compileGLSLToSPIRV(missShaderGLSL, shaderc_miss_shader, "Miss Shader");
	auto closestHitShaderSPIRV = Shader::compileGLSLToSPIRV(closestHitShaderGLSL, shaderc_closesthit_shader, "Closest Hit Shader");
    auto intersectionShaderSPIRV = Shader::compileGLSLToSPIRV(intersectionShaderGLSL, shaderc_intersection_shader, "Intersection Shader");

	VkShaderModule raygenShaderModule = Shader::createShaderModule(device, raygenShaderSPIRV);
	VkShaderModule missShaderModule = Shader::createShaderModule(device, missShaderSPIRV);
	VkShaderModule closestHitShaderModule = Shader::createShaderModule(device, closestHitShaderSPIRV);
    VkShaderModule intersectionShaderModule = Shader::createShaderModule(device, intersectionShaderSPIRV);

	// Ray generation group
	{
		VkPipelineShaderStageCreateInfo shaderStageCreateInfo{};
		shaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageCreateInfo.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
		shaderStageCreateInfo.module = raygenShaderModule;
		shaderStageCreateInfo.pName = "main"; // specify the entry point
		shaderStageCreateInfo.pSpecializationInfo = nullptr; // For setting constants at pipeline creation
		shaderStages.push_back(shaderStageCreateInfo);

		VkRayTracingShaderGroupCreateInfoKHR raygenGroupCreateInfo{};
		raygenGroupCreateInfo.sType              = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
		raygenGroupCreateInfo.type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
		raygenGroupCreateInfo.generalShader		 = static_cast<uint32_t>(shaderStages.size()) - 1;
		raygenGroupCreateInfo.closestHitShader   = VK_SHADER_UNUSED_KHR;
		raygenGroupCreateInfo.anyHitShader       = VK_SHADER_UNUSED_KHR;
		raygenGroupCreateInfo.intersectionShader = VK_SHADER_UNUSED_KHR;

		shaderGroups.push_back(raygenGroupCreateInfo);
	}

	// Ray miss group
	{
		VkPipelineShaderStageCreateInfo shaderStageCreateInfo{};
		shaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageCreateInfo.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
		shaderStageCreateInfo.module = missShaderModule;
		shaderStageCreateInfo.pName = "main"; // specify the entry point
		shaderStageCreateInfo.pSpecializationInfo = nullptr; // For setting constants at pipeline creation
		shaderStages.push_back(shaderStageCreateInfo);

		VkRayTracingShaderGroupCreateInfoKHR raygenGroupCreateInfo{};
		raygenGroupCreateInfo.sType              = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
		raygenGroupCreateInfo.type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
		raygenGroupCreateInfo.generalShader		 = static_cast<uint32_t>(shaderStages.size()) - 1;
		raygenGroupCreateInfo.closestHitShader   = VK_SHADER_UNUSED_KHR;
		raygenGroupCreateInfo.anyHitShader       = VK_SHADER_UNUSED_KHR;
		raygenGroupCreateInfo.intersectionShader = VK_SHADER_UNUSED_KHR;

		shaderGroups.push_back(raygenGroupCreateInfo);
	}
	
	// Procedural Hit group
	{
		// Intersection Shader Stage
		VkPipelineShaderStageCreateInfo intersectionStageInfo{};
		intersectionStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		intersectionStageInfo.stage = VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
		intersectionStageInfo.module = intersectionShaderModule;
		intersectionStageInfo.pName = "main";
		shaderStages.push_back(intersectionStageInfo);

		VkPipelineShaderStageCreateInfo closestHitStageInfo{};
		closestHitStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		closestHitStageInfo.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
		closestHitStageInfo.module = closestHitShaderModule;
		closestHitStageInfo.pName = "main";
		shaderStages.push_back(closestHitStageInfo);

		VkRayTracingShaderGroupCreateInfoKHR proceduralHitGroup{};
		proceduralHitGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
		proceduralHitGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR;
		proceduralHitGroup.generalShader = VK_SHADER_UNUSED_KHR;
		proceduralHitGroup.closestHitShader = static_cast<uint32_t>(shaderStages.size()) - 1; // Index of closest hit
		proceduralHitGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
		proceduralHitGroup.intersectionShader = static_cast<uint32_t>(shaderStages.size()) - 2; // Index of intersection

		shaderGroups.push_back(proceduralHitGroup);
	}

	VkRayTracingPipelineCreateInfoKHR raytracingPipelineCreateInfo{};
	raytracingPipelineCreateInfo.sType                        = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
	raytracingPipelineCreateInfo.stageCount                   = static_cast<uint32_t>(shaderStages.size());
	raytracingPipelineCreateInfo.pStages                      = shaderStages.data();
	raytracingPipelineCreateInfo.groupCount                   = static_cast<uint32_t>(shaderGroups.size());
	raytracingPipelineCreateInfo.pGroups                      = shaderGroups.data();
	raytracingPipelineCreateInfo.maxPipelineRayRecursionDepth = 1;
	raytracingPipelineCreateInfo.layout                       = rtPipelineLayout;

	if (vkCreateRayTracingPipelinesKHR(device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &raytracingPipelineCreateInfo, nullptr, &rtPipeline) != VK_SUCCESS)
	{
		throw std::runtime_error("could not create the ray tracing pipelien");
	}
	
	vkDestroyShaderModule(device, raygenShaderModule, nullptr);
	vkDestroyShaderModule(device, missShaderModule, nullptr);
	vkDestroyShaderModule(device, closestHitShaderModule, nullptr);
	vkDestroyShaderModule(device, intersectionShaderModule, nullptr);
}

void VoxelEngine::createUniformBuffer()
{
    const VkDeviceSize bufferSize = sizeof(UniformData);
    
    ubo.create(
        vmaAllocator,
        device,
        bufferSize,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | 
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        ManagedBuffer::BufferType::HostVisible
    );

    uniformData.view_inverse = glm::mat4(1.0f);
    uniformData.proj_inverse = glm::mat4(1.0f);
    
    ubo.updateData(vmaAllocator, &uniformData, sizeof(UniformData));
}

void VoxelEngine::createSyncObjects() 
{
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    imagesInFlight.resize(swapChainImages.size(), VK_NULL_HANDLE); 

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; 

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) 
	{ 
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) 
		{
            throw std::runtime_error("failed to create synchronization objects!");
        }
    }
}

void VoxelEngine::createSphereBuffer()
{
    VkDeviceSize bufferSize = sizeof(Sphere) * spheres.size();
    sphereBuffer.create(
        vmaAllocator, device,
        bufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        ManagedBuffer::BufferType::HostVisible
    );
    sphereBuffer.updateData(vmaAllocator, spheres.data(), bufferSize);
}

void VoxelEngine::recordCommandBufferRT(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
	VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    // Transition swapchain image from undefined layout to TRANSFER_DST_OPTIMAL
    VkImageMemoryBarrier swapchainBarrierBefore{};
    swapchainBarrierBefore.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    swapchainBarrierBefore.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    swapchainBarrierBefore.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    swapchainBarrierBefore.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    swapchainBarrierBefore.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    swapchainBarrierBefore.image = swapChainImages[imageIndex];
    swapchainBarrierBefore.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    swapchainBarrierBefore.subresourceRange.baseMipLevel = 0;
    swapchainBarrierBefore.subresourceRange.levelCount = 1;
    swapchainBarrierBefore.subresourceRange.baseArrayLayer = 0;
    swapchainBarrierBefore.subresourceRange.layerCount = 1;
    swapchainBarrierBefore.srcAccessMask = 0;
    swapchainBarrierBefore.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &swapchainBarrierBefore
    );

    // Bind ray tracing pipeline and descriptor sets
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline);
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
        rtPipelineLayout,
        0,
        1,
        &descriptorSetsRT[currentFrame],
        0,
        nullptr
    );

	const uint32_t handleSize = rtProperties.shaderGroupHandleSize;
    const uint32_t handleSizeAligned = alignedSize(handleSize, rtProperties.shaderGroupBaseAlignment);

    // Trace rays
	VkStridedDeviceAddressRegionKHR raygenSbtRegion{};
    raygenSbtRegion.deviceAddress = raygenShaderBindingTable.deviceAddress;
    raygenSbtRegion.stride = handleSizeAligned; // Correct stride
    raygenSbtRegion.size = handleSizeAligned;   // Correct size

    VkStridedDeviceAddressRegionKHR missSbtRegion{};
    missSbtRegion.deviceAddress = missShaderBindingTable.deviceAddress;
    missSbtRegion.stride = handleSizeAligned;
    missSbtRegion.size = handleSizeAligned;

    VkStridedDeviceAddressRegionKHR hitSbtRegion{};
    hitSbtRegion.deviceAddress = closestHitShaderBindingTable.deviceAddress;
    hitSbtRegion.stride = handleSizeAligned;
    hitSbtRegion.size = handleSizeAligned;

    VkStridedDeviceAddressRegionKHR callableSbtRegion{};

    vkCmdTraceRaysKHR(
        commandBuffer,
        &raygenSbtRegion,
        &missSbtRegion,
        &hitSbtRegion,
        &callableSbtRegion,
        swapChainExtent.width,
        swapChainExtent.height,
        1
    );

    // Barrier to ensure ray tracing writes finish before copy
    VkImageMemoryBarrier storageImageBarrier{};
    storageImageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    storageImageBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    storageImageBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    storageImageBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    storageImageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    storageImageBarrier.image = storageImages[currentFrame].image;
    storageImageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    storageImageBarrier.subresourceRange.baseMipLevel = 0;
    storageImageBarrier.subresourceRange.levelCount = 1;
    storageImageBarrier.subresourceRange.baseArrayLayer = 0;
    storageImageBarrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &storageImageBarrier
    );

    // Copy storage image to swapchain image
    VkImageCopy copyRegion{};
    copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.srcSubresource.mipLevel = 0;
    copyRegion.srcSubresource.baseArrayLayer = 0;
    copyRegion.srcSubresource.layerCount = 1;
    copyRegion.dstSubresource = copyRegion.srcSubresource;
    copyRegion.extent = { swapChainExtent.width, swapChainExtent.height, 1 };

    vkCmdCopyImage(
        commandBuffer,
        storageImages[currentFrame].image,
        VK_IMAGE_LAYOUT_GENERAL,
        swapChainImages[imageIndex],
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &copyRegion
    );

    // Transition swapchain image to PRESENT_SRC layout
    VkImageMemoryBarrier swapchainBarrierAfter{};
    swapchainBarrierAfter.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    swapchainBarrierAfter.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    swapchainBarrierAfter.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    swapchainBarrierAfter.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    swapchainBarrierAfter.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    swapchainBarrierAfter.image = swapChainImages[imageIndex];
    swapchainBarrierAfter.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    swapchainBarrierAfter.subresourceRange.baseMipLevel = 0;
    swapchainBarrierAfter.subresourceRange.levelCount = 1;
    swapchainBarrierAfter.subresourceRange.baseArrayLayer = 0;
    swapchainBarrierAfter.subresourceRange.layerCount = 1;
    swapchainBarrierAfter.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    swapchainBarrierAfter.dstAccessMask = 0;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &swapchainBarrierAfter
    );

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }	

}

void VoxelEngine::updateUniformBuffersRT()
{
	if (!camera.getWasUpdated()) return; // No need to update buffers when the camera hasn't changed

	camera.updateView();
	camera.setWasUpdated(false);
			
	uniformData.view_inverse = camera.getInvView();
	uniformData.proj_inverse = camera.getInvProj();

	if (ubo.type == ManagedBuffer::BufferType::HostVisible)
	{
        void* mapped;
		vmaMapMemory(vmaAllocator, ubo.allocation, &mapped);
		memcpy(mapped, &uniformData, sizeof(UniformData));
		vmaUnmapMemory(vmaAllocator, ubo.allocation);
    } 
	else 
	{
        throw std::runtime_error("Trying to update non-host-visible uniform buffer");
    }
}

void VoxelEngine::createCamera()
{
	camera.setPerspective(FOV, static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height));
	camera.setPosition({0.0f, 0.0f, 5.0f});
    camera.lookAt({0.0f, 0.0f, 0.0f});	
}

void VoxelEngine::handleInput(GLFWwindow* window)
{
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.move(camera.getForward() * 0.1f);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.move(-camera.getForward() * 0.1f);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.move(-camera.getRight() * 0.1f);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.move(camera.getRight() * 0.1f);
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) 
	{
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        firstMouse = true;
    }
    if (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS) 
	{
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
}


void VoxelEngine::recreateSwapChain() {
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }

	if (!commandBuffers.empty()) 
	{
        vkFreeCommandBuffers(device, commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
    }
    createCommandBuffers();

    vkDeviceWaitIdle(device);  // Ensure all operations complete

    cleanupSwapChain();
	cleanupSyncObjects();

    // Recreate swapchain resources
    createSwapChain();
    createImageViews();
    createStorageImages();
	createSyncObjects();
    createCommandBuffers(); 

	imagesInFlight.resize(swapChainImages.size(), VK_NULL_HANDLE);

    // Reset frame index to prevent out-of-bounds access
    currentFrame = 0;

    // Update camera and descriptors
    camera.setPerspective(FOV, 
        static_cast<float>(swapChainExtent.width) / 
        static_cast<float>(swapChainExtent.height));
    updateDescriptorSetsRT();
}

void VoxelEngine::drawFrameRT() 
{
	vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(
        device, 
        swapChain, 
        UINT64_MAX, 
        imageAvailableSemaphores[currentFrame], 
        VK_NULL_HANDLE, 
        &imageIndex
    );

    if (result == VK_ERROR_OUT_OF_DATE_KHR || framebufferResized) {
        framebufferResized = false;
        recreateSwapChain();
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    // Check if previous frame using this image is complete
    if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }
    imagesInFlight[imageIndex] = inFlightFences[currentFrame];

    updateUniformBuffersRT();

    // Use command buffer specific to this swapchain image
    VkCommandBuffer commandBuffer = commandBuffers[currentFrame];
    
    recordCommandBufferRT(commandBuffer, imageIndex);

    VkSemaphoreSubmitInfoKHR waitSemaphoreSubmitInfo{};
	waitSemaphoreSubmitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR;
	waitSemaphoreSubmitInfo.semaphore = imageAvailableSemaphores[currentFrame];
	// Correct the wait stage to COLOR_ATTACHMENT_OUTPUT
	waitSemaphoreSubmitInfo.stageMask = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
	waitSemaphoreSubmitInfo.deviceIndex = 0;

    VkSemaphoreSubmitInfoKHR signalSemaphoreSubmitInfo{};
    signalSemaphoreSubmitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR;
    signalSemaphoreSubmitInfo.semaphore = renderFinishedSemaphores[currentFrame];
    signalSemaphoreSubmitInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    signalSemaphoreSubmitInfo.deviceIndex = 0;

    VkCommandBufferSubmitInfoKHR commandBufferSubmitInfo{};
    commandBufferSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO_KHR;
    commandBufferSubmitInfo.commandBuffer = commandBuffer;
    commandBufferSubmitInfo.deviceMask = 0;

    VkSubmitInfo2KHR submitInfo2{};
    submitInfo2.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR;
    submitInfo2.waitSemaphoreInfoCount = 1;
    submitInfo2.pWaitSemaphoreInfos = &waitSemaphoreSubmitInfo;
    submitInfo2.commandBufferInfoCount = 1;
    submitInfo2.pCommandBufferInfos = &commandBufferSubmitInfo;
    submitInfo2.signalSemaphoreInfoCount = 1;
    submitInfo2.pSignalSemaphoreInfos = &signalSemaphoreSubmitInfo;

    vkResetFences(device, 1, &inFlightFences[currentFrame]);
    if (VoxelEngine::vkQueueSubmit2KHR(graphicsQueue, 1, &submitInfo2, inFlightFences[currentFrame]) != VK_SUCCESS)
	{
        throw std::runtime_error("failed to submit command buffer!");
    }


    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinishedSemaphores[currentFrame];

    VkSwapchainKHR swapChains[] = {swapChain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
        framebufferResized = false;
        recreateSwapChain();
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to present swap chain image!");
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VoxelEngine::cleanupSyncObjects() 
{
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
        vkDestroyFence(device, inFlightFences[i], nullptr);
    }
    imageAvailableSemaphores.clear();
    renderFinishedSemaphores.clear();
    inFlightFences.clear();
}


void VoxelEngine::cleanup()
{
	cleanupRayTracing();

	cleanupSwapChain();
	vkDestroyCommandPool(device, commandPool, nullptr);

	if (descriptorPool != VK_NULL_HANDLE) vkDestroyDescriptorPool(device, descriptorPool, nullptr);

	vmaDestroyAllocator(vmaAllocator);
	vkDestroyDevice(device, nullptr);

	if (enableValidationLayers)
	{
		DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
	}

	vkDestroySurfaceKHR(instance, surface, nullptr);
	vkDestroyInstance(instance, nullptr);

	glfwDestroyWindow(window);
	glfwTerminate();
}

void VoxelEngine::cleanupSwapChain()
{
	for (size_t i = 0; i < swapChainImageViews.size(); ++i)
	{
		vkDestroyImageView(device, swapChainImageViews[i], nullptr);
	}
	swapChainImageViews.clear();
	vkDestroySwapchainKHR(device, swapChain, nullptr);

	if (!commandBuffers.empty()) 
	{
        vkFreeCommandBuffers(device, commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
        commandBuffers.clear();
    }
}


void VoxelEngine::cleanupRayTracing()
{
	if (topLevelAccelerationStructure.handle != VK_NULL_HANDLE)
	{
		deleteAS(topLevelAccelerationStructure);
	}
	if (bottomLevelAccelerationStructure.handle != VK_NULL_HANDLE)
	{
		deleteAS(bottomLevelAccelerationStructure);
	}

	if (rtPipeline != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(device, rtPipeline, nullptr);
	}

	if (rtPipelineLayout != VK_NULL_HANDLE)
	{
		vkDestroyPipelineLayout(device, rtPipelineLayout, nullptr);
	}

	if (descriptorSetLayoutRT != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorSetLayout(device, descriptorSetLayoutRT, nullptr);
	}
	

	raygenShaderBindingTable.destroy(vmaAllocator);
	missShaderBindingTable.destroy(vmaAllocator);
	closestHitShaderBindingTable.destroy(vmaAllocator);

	for (auto& storageImage : storageImages)
	{
		storageImage.destroy();
	}
	storageImages.clear();

	ubo.destroy(vmaAllocator);
	sphereBuffer.destroy(vmaAllocator);
}

void ScratchBuffer::createScratchBuffer(VmaAllocator allocator, VkDevice device, VkDeviceSize size, ScratchBuffer& scratchBuffer)
{
	// Define buffer create info
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	// Define allocation info
	VmaAllocationCreateInfo allocInfo{};
	allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY; // Store in GPU memory
	allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT; // Fast GPU access

	// Create buffer with VMA
	vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &scratchBuffer.handle, &scratchBuffer.allocation, nullptr);

	// Get device address
	VkBufferDeviceAddressInfo addressInfo{};
	addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	addressInfo.buffer = scratchBuffer.handle;

	scratchBuffer.deviceAddress = VoxelEngine::vkGetBufferDeviceAddressKHR(device, &addressInfo);
}

void ScratchBuffer::destroyScratchBuffer(VmaAllocator allocator, ScratchBuffer& scratchBuffer)
{
	if (scratchBuffer.handle != VK_NULL_HANDLE)
	{
		vmaDestroyBuffer(allocator, scratchBuffer.handle, scratchBuffer.allocation);
	}
}


void ManagedBuffer::create(VmaAllocator allocator, VkDevice device, VkDeviceSize bufferSize, VkBufferUsageFlags usage, BufferType bufferType)
{
	if (handle != VK_NULL_HANDLE)
	{
		throw std::runtime_error("Buffer already created");
	}

	if (allocator == VK_NULL_HANDLE)
	{
		throw std::runtime_error("Allocator is null handle");
	}

	size = bufferSize;
	type = bufferType;

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage | (type == BufferType::DeviceLocal ?
		VK_BUFFER_USAGE_TRANSFER_DST_BIT : 0);
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo allocInfo{};
	allocInfo.usage = (type == BufferType::HostVisible) ?
		VMA_MEMORY_USAGE_CPU_TO_GPU :
		VMA_MEMORY_USAGE_GPU_ONLY;

	if (vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &handle, &allocation, nullptr) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create buffer");
	}

	if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
	{
		VkBufferDeviceAddressInfo addressInfo{};
		addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		addressInfo.buffer = handle;
		deviceAddress = VoxelEngine::vkGetBufferDeviceAddressKHR(device, &addressInfo);
	}

	_isDestroyed = false;
}


 void ManagedBuffer::uploadData(VmaAllocator allocator, VkDevice device, VkCommandBuffer commandBuffer, VkQueue queue, const void* data, VkDeviceSize dataSize, VkDeviceSize offset)
{
	if (type != BufferType::DeviceLocal)
	{
		throw std::runtime_error("UploadData only valid for device-local buffers");
	}

	// Create temporary staging buffer
	ManagedBuffer staging;
	staging.create(allocator, device, dataSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		BufferType::HostVisible);

	// Map and copy to staging buffer
	void* mapped;
	vmaMapMemory(allocator, staging.allocation, &mapped);
	memcpy(mapped, data, dataSize);
	vmaUnmapMemory(allocator, staging.allocation);

	// Transfer commands
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = offset;
	copyRegion.size = dataSize;
	vkCmdCopyBuffer(commandBuffer, staging.handle, handle, 1, &copyRegion);

	vkEndCommandBuffer(commandBuffer);

	// Submit and wait
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	VkFence fence;
	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	vkCreateFence(device, &fenceInfo, nullptr, &fence);

	vkQueueSubmit(queue, 1, &submitInfo, fence);
	vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);

	// Cleanup
	vkDestroyFence(device, fence, nullptr);
	staging.destroy(allocator);
}
