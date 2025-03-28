#include "VoxelEngine.hpp"

#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"


std::string Shader::readShaderFile(const std::string& path)
{
	std::ifstream file(path);
	if (!file.is_open())
	{
        LOG_ERROR("Failed to open shader file: " + path);
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
		LOG_ERROR("Shader compilation failed: " + std::string(module.GetErrorMessage()));
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
		LOG_ERROR("Failed to create shader module!");
	}

	return shaderModule;
}

void VoxelEngine::mouseCallback(GLFWwindow* window, double xpos, double ypos)
{
    VoxelEngine* app = static_cast<VoxelEngine*>(glfwGetWindowUserPointer(window));

	if (glfwGetInputMode(app->window, GLFW_CURSOR) == GLFW_CURSOR_NORMAL)
	{
		return;
	}

    if (app->firstMouse) {
        app->lastMouseX = xpos;
        app->lastMouseY = ypos;
        app->firstMouse = false;
    }
    
    float xoffset = xpos - app->lastMouseX;
    float yoffset = app->lastMouseY - ypos; 
    app->lastMouseX = xpos;
    app->lastMouseY = ypos;
    
	app->fpsCamera.look(xoffset, -yoffset);
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

void VoxelEngine::createSwapChain()
{
	VulkanContext::SwapChainSupportDetails swapChainSupport = VulkanContext::QuerySwapChainSupport(VulkanContext::physicalDevice);

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
	createInfo.surface = VulkanContext::surface;

	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	VulkanContext::QueueFamilyIndices indices = VulkanContext::FindQueueFamilies(VulkanContext::physicalDevice);
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

	if (vkCreateSwapchainKHR(VulkanContext::device, &createInfo, nullptr, &swapChain) != VK_SUCCESS)
	{
		LOG_ERROR("failed to create swap chain!");
	}

	vkGetSwapchainImagesKHR(VulkanContext::device, swapChain, &imageCount, nullptr); // we specify the minimum number of images but the implementation can create more, thus here we check the actual image count
	swapChainImages.resize(imageCount); // Resize to match correct image count
	vkGetSwapchainImagesKHR(VulkanContext::device, swapChain, &imageCount, swapChainImages.data()); // now we can specify the data as the vector is the correct size

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

		if (vkCreateImageView(VulkanContext::device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS)
		{
			LOG_ERROR("failed to create image views!");
		}
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
		LOG_ERROR("Failed to allocate command buffer");
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
	VulkanContext::vkQueueSubmit2KHR(queue, 1, &finalSubmit, fence);

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
	allocInfo.commandPool = VulkanContext::commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

	if (vkAllocateCommandBuffers(VulkanContext::device, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
	{
		LOG_ERROR("failed to allocate command buffers!");
	}

}

uint64_t VoxelEngine::getBufferDeviceAddress(VkBuffer buffer)
{
	VkBufferDeviceAddressInfoKHR bufferDeviceAddressInfo{};
	bufferDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	bufferDeviceAddressInfo.buffer = buffer;
	
	return VulkanContext::vkGetBufferDeviceAddressKHR(VulkanContext::device, &bufferDeviceAddressInfo);
}

void VoxelEngine::createStorageImages()
{
		outputImage.destroy();
		debugImage.destroy();

        VkExtent3D extent;
        extent.depth = 1;
        extent.width = swapChainExtent.width;
        extent.height = swapChainExtent.height;

		
        debugImage.create(VulkanContext::vmaAllocator, VulkanContext::device, extent, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
		outputImage.create(VulkanContext::vmaAllocator, VulkanContext::device, extent, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT);

        // Transition layout from UNDEFINED to GENERAL
        VkCommandBuffer cmd = createCommandBuffer(VulkanContext::device, VulkanContext::commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
        
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = debugImage.image;
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


		barrier.image = outputImage.image;
		vkCmdPipelineBarrier(
			cmd,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);

        flushCommandBuffer(VulkanContext::device, cmd, VulkanContext::graphicsQueue, VulkanContext::commandPool, true);
}

void VoxelEngine::createBLAS()
{
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    auto randFloat = [](float min, float max)
        {
        float random = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
        return min + random * (max - min);
    };

    const int numRandomSpheres = 500000; 
    for (int i = 0; i < numRandomSpheres; ++i)
    {
        float x = randFloat(-2500.0f, 2500.0f);
        float y = randFloat(-2500.0f, 2500.0f);
        float z = randFloat(-2500.0f, 2500.0f);
        float radius = randFloat(0.5f, 6.0f);
        spheres.push_back({ {x, y, z, radius} });
    }

    uint32_t firstDynamicBlasIndex = static_cast<uint32_t>(-1); 

    std::vector<VkAabbPositionsKHR> aabbVec;
    for (size_t i = 0; i < spheres.size(); ++i)
    {
        const auto& sphere = spheres[i];

        VkAabbPositionsKHR aabb = {
            sphere.positionRadius.x - sphere.positionRadius.w,
            sphere.positionRadius.y - sphere.positionRadius.w,
            sphere.positionRadius.z - sphere.positionRadius.w,
            sphere.positionRadius.x + sphere.positionRadius.w,
            sphere.positionRadius.y + sphere.positionRadius.w,
            sphere.positionRadius.z + sphere.positionRadius.w,
        };

        aabbVec.push_back(aabb);
    }

	accelerationStructureManager.addBlas(aabbVec.data(), sizeof(VkAabbPositionsKHR) * aabbVec.size());

    VkTransformMatrixKHR transform = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
    };

   uint32_t instanceIndex = accelerationStructureManager.instantiateBlas(0, transform);

	if (firstDynamicBlasIndex == static_cast<uint32_t>(-1))
	{
		 firstDynamicBlasIndex = instanceIndex;
	}

    if (firstDynamicBlasIndex != static_cast<uint32_t>(-1)) 
    {
        movingIndex = firstDynamicBlasIndex; 
    } 
    else if (!spheres.empty()) 
    {
        movingIndex = 0; 
    }

    createSphereBuffer();    
}


void VoxelEngine::createTLAS()
{
    accelerationStructureManager.initTLAS();
}

void VoxelEngine::createShaderBindingTables()
{
    const uint32_t handleSize = VulkanContext::rtProperties.shaderGroupHandleSize;
    const uint32_t handleSizeAligned = alignedSize(VulkanContext::rtProperties.shaderGroupHandleSize, 
                                             VulkanContext::rtProperties.shaderGroupBaseAlignment);
    const uint32_t groupCount = static_cast<uint32_t>(shaderGroups.size());
    
    VkDeviceSize sbtSize = groupCount * handleSizeAligned;
    
    raygenShaderBindingTable.create(VulkanContext::vmaAllocator, VulkanContext::device, handleSizeAligned, 
        VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | 
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | 
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        ManagedBuffer::BufferType::DeviceLocal);
        
    missShaderBindingTable.create(VulkanContext::vmaAllocator, VulkanContext::device, handleSizeAligned,
        VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | 
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | 
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        ManagedBuffer::BufferType::DeviceLocal);
        
    closestHitShaderBindingTable.create(VulkanContext::vmaAllocator, VulkanContext::device, handleSizeAligned, 
        VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | 
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | 
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        ManagedBuffer::BufferType::DeviceLocal);

    // Get shader handles
    std::vector<uint8_t> shaderHandleStorage(groupCount * handleSize);
    VulkanContext::vkGetRayTracingShaderGroupHandlesKHR(
        VulkanContext::device, rtPipeline, 0, groupCount, groupCount * handleSize, shaderHandleStorage.data()
    );
    
    ManagedBuffer stagingBuffer;
    stagingBuffer.create(VulkanContext::vmaAllocator, VulkanContext::device, sbtSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        ManagedBuffer::BufferType::HostVisible);
    
    uint8_t* data = nullptr;
    vmaMapMemory(VulkanContext::vmaAllocator, stagingBuffer.allocation, (void**)&data);
    
    for (uint32_t i = 0; i < groupCount; i++) {
        memcpy(data + i * handleSizeAligned, shaderHandleStorage.data() + i * handleSize, handleSize);
    }
    
    vmaUnmapMemory(VulkanContext::vmaAllocator, stagingBuffer.allocation);
    
    VkCommandBuffer cmd = createCommandBuffer(VulkanContext::device, VulkanContext::commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    
    VkBufferCopy copyRegion{};
    copyRegion.size = handleSizeAligned;
    
    // Raygen (group 0)
    copyRegion.srcOffset = 0;
    vkCmdCopyBuffer(cmd, stagingBuffer.handle, raygenShaderBindingTable.handle, 1, &copyRegion);
    
    // Miss (group 1)
    copyRegion.srcOffset = handleSizeAligned;
    vkCmdCopyBuffer(cmd, stagingBuffer.handle, missShaderBindingTable.handle, 1, &copyRegion);
    
    // Hit (group 2)
    copyRegion.srcOffset = 2 * handleSizeAligned;
    vkCmdCopyBuffer(cmd, stagingBuffer.handle, closestHitShaderBindingTable.handle, 1, &copyRegion);
    
    flushCommandBuffer(VulkanContext::device, cmd, VulkanContext::graphicsQueue, VulkanContext::commandPool, true);
    stagingBuffer.destroy(VulkanContext::vmaAllocator);
    
    raygenShaderBindingTable.deviceAddress = getBufferDeviceAddress(raygenShaderBindingTable.handle);
    missShaderBindingTable.deviceAddress = getBufferDeviceAddress(missShaderBindingTable.handle);
    closestHitShaderBindingTable.deviceAddress = getBufferDeviceAddress(closestHitShaderBindingTable.handle);
}


void VoxelEngine::createDescriptorSetsRT() 
{
    std::array<VkDescriptorPoolSize, 4> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    poolSizes[0].descriptorCount = MAX_FRAMES_IN_FLIGHT;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[1].descriptorCount = MAX_FRAMES_IN_FLIGHT * 2; // For output and debug images
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[2].descriptorCount = MAX_FRAMES_IN_FLIGHT;
    poolSizes[3].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[3].descriptorCount = MAX_FRAMES_IN_FLIGHT;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT; 

    VK_ERROR_CHECK(vkCreateDescriptorPool(VulkanContext::device, &poolInfo, nullptr, &descriptorPool));

    descriptorSetsRT.resize(MAX_FRAMES_IN_FLIGHT);
    
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayoutRT);
    
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
    allocInfo.pSetLayouts = layouts.data();

    VK_ERROR_CHECK(vkAllocateDescriptorSets(VulkanContext::device, &allocInfo, descriptorSetsRT.data()));

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) 
	{
        updateDescriptorSetRT(i);
    }
}

void VoxelEngine::updateDescriptorSetRT(uint32_t index) 
{
    std::vector<VkWriteDescriptorSet> descriptorWrites;

    VkWriteDescriptorSetAccelerationStructureKHR accelWriteInfo{};
    accelWriteInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    accelWriteInfo.accelerationStructureCount = 1;
    //accelWriteInfo.pAccelerationStructures = &topLevelAccelerationStructure.handle;

    VkAccelerationStructureKHR tlasHandle = accelerationStructureManager.getTLASHandle();
    accelWriteInfo.pAccelerationStructures = &tlasHandle;

    VkWriteDescriptorSet accelWrite{};
    accelWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    accelWrite.pNext = &accelWriteInfo;
    accelWrite.dstSet = descriptorSetsRT[index];
    accelWrite.dstBinding = 0;
    accelWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    accelWrite.descriptorCount = 1;
    descriptorWrites.push_back(accelWrite);

    VkDescriptorImageInfo outputImageInfo{};
    outputImageInfo.imageView = outputImage.view;
    outputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet outputImageWrite{};
    outputImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    outputImageWrite.dstSet = descriptorSetsRT[index];
    outputImageWrite.dstBinding = 1;
    outputImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    outputImageWrite.descriptorCount = 1;
    outputImageWrite.pImageInfo = &outputImageInfo;
    descriptorWrites.push_back(outputImageWrite);

    VkDescriptorBufferInfo uniformBufferInfo{};
    uniformBufferInfo.buffer = uniformBuffersRT[index].handle;
    uniformBufferInfo.offset = 0;
    uniformBufferInfo.range = sizeof(UniformData);

    VkWriteDescriptorSet uniformWrite{};
    uniformWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    uniformWrite.dstSet = descriptorSetsRT[index];
    uniformWrite.dstBinding = 2;
    uniformWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uniformWrite.descriptorCount = 1;
    uniformWrite.pBufferInfo = &uniformBufferInfo;
    descriptorWrites.push_back(uniformWrite);

    VkDescriptorBufferInfo sphereBufferInfo{};
    sphereBufferInfo.buffer = sphereBuffer.handle;
    sphereBufferInfo.offset = 0;
    sphereBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet sphereWrite{};
    sphereWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    sphereWrite.dstSet = descriptorSetsRT[index];
    sphereWrite.dstBinding = 3;
    sphereWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    sphereWrite.descriptorCount = 1;
    sphereWrite.pBufferInfo = &sphereBufferInfo;
    descriptorWrites.push_back(sphereWrite);

    VkDescriptorImageInfo debugImageInfo{};
	debugImageInfo.imageView = debugImage.view;
	debugImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet debugImageWrite{};
    debugImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    debugImageWrite.dstSet = descriptorSetsRT[index];
    debugImageWrite.dstBinding = 4;
    debugImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    debugImageWrite.descriptorCount = 1;
    debugImageWrite.pImageInfo = &debugImageInfo;
    descriptorWrites.push_back(debugImageWrite);

    vkUpdateDescriptorSets(VulkanContext::device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

void VoxelEngine::updateDescriptorSetsRT() 
{
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		updateDescriptorSetRT(i);
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

	VkDescriptorSetLayoutBinding accumulationImageLayoutBinding{};
    accumulationImageLayoutBinding.binding = 4; 
    accumulationImageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    accumulationImageLayoutBinding.descriptorCount = 1;
    accumulationImageLayoutBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	

	std::vector<VkDescriptorSetLayoutBinding> bindings = {
	    accelerationStructureLayoutBinding,
	    resultImageLayoutBinding,
		uniformBufferBinding,
		sphereBufferBinding,
		accumulationImageLayoutBinding
	};


	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings    = bindings.data();

	VK_ERROR_CHECK(vkCreateDescriptorSetLayout(VulkanContext::device, &layoutInfo, nullptr, &descriptorSetLayoutRT));

	VkPushConstantRange pushConstantRange = {};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR; // Include closest hit stage
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(Constants);

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayoutRT;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

	VK_ERROR_CHECK(vkCreatePipelineLayout(VulkanContext::device, &pipelineLayoutCreateInfo, nullptr, &rtPipelineLayout));

	std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

	auto raygenShaderGLSL = Shader::readShaderFile("C:/Users/Bennett/source/repos/VoxelEngine/shaders/shader.rgen");
	auto missShaderGLSL = Shader::readShaderFile("C:/Users/Bennett/source/repos/VoxelEngine/shaders/shader.rmiss");
	auto closestHitShaderGLSL = Shader::readShaderFile("C:/Users/Bennett/source/repos/VoxelEngine/shaders/shader.rchit");
	auto intersectionShaderGLSL = Shader::readShaderFile("C:/Users/Bennett/source/repos/VoxelEngine/shaders/shader.rint");

	auto raygenShaderSPIRV = Shader::compileGLSLToSPIRV(raygenShaderGLSL, shaderc_raygen_shader, "Raygen Shader");
	auto missShaderSPIRV = Shader::compileGLSLToSPIRV(missShaderGLSL, shaderc_miss_shader, "Miss Shader");
	auto closestHitShaderSPIRV = Shader::compileGLSLToSPIRV(closestHitShaderGLSL, shaderc_closesthit_shader, "Closest Hit Shader");
    auto intersectionShaderSPIRV = Shader::compileGLSLToSPIRV(intersectionShaderGLSL, shaderc_intersection_shader, "Intersection Shader");

	VkShaderModule raygenShaderModule = Shader::createShaderModule(VulkanContext::device, raygenShaderSPIRV);
	VkShaderModule missShaderModule = Shader::createShaderModule(VulkanContext::device, missShaderSPIRV);
	VkShaderModule closestHitShaderModule = Shader::createShaderModule(VulkanContext::device, closestHitShaderSPIRV);
    VkShaderModule intersectionShaderModule = Shader::createShaderModule(VulkanContext::device, intersectionShaderSPIRV);

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
		proceduralHitGroup.closestHitShader = static_cast<uint32_t>(shaderStages.size()) - 1; 
		proceduralHitGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
		proceduralHitGroup.intersectionShader = static_cast<uint32_t>(shaderStages.size()) - 2; 

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

	VK_ERROR_CHECK(VulkanContext::vkCreateRayTracingPipelinesKHR(VulkanContext::device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &raytracingPipelineCreateInfo, nullptr, &rtPipeline));
	
	vkDestroyShaderModule(VulkanContext::device, raygenShaderModule, nullptr);
	vkDestroyShaderModule(VulkanContext::device, missShaderModule, nullptr);
	vkDestroyShaderModule(VulkanContext::device, closestHitShaderModule, nullptr);
	vkDestroyShaderModule(VulkanContext::device, intersectionShaderModule, nullptr);
}

void VoxelEngine::createUniformBuffers()
{
    const VkDeviceSize bufferSize = sizeof(UniformData);

	uniformBuffersRT.resize(MAX_FRAMES_IN_FLIGHT);
	uniformData.view_inverse = glm::mat4(1.0f);
    uniformData.proj_inverse = glm::mat4(1.0f);
	uniformData.position = glm::vec3(0.f, 0.f, 0.f);


	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		uniformBuffersRT[i].create(VulkanContext::vmaAllocator, VulkanContext::device, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, ManagedBuffer::BufferType::HostVisible);
		uniformBuffersRT[i].updateData(VulkanContext::vmaAllocator, &uniformData, bufferSize);
	}
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
        if (vkCreateSemaphore(VulkanContext::device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(VulkanContext::device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(VulkanContext::device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) 
		{
            LOG_ERROR("failed to create synchronization objects!");
        }
    }
}

void VoxelEngine::createSphereBuffer()
{
    VkDeviceSize bufferSize = sizeof(Sphere) * spheres.size();
    sphereBuffer.create(VulkanContext::vmaAllocator, VulkanContext::device, bufferSize, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VulkanContext::vkGetBufferDeviceAddressKHR);

    sphereBuffer.uploadData(VulkanContext::vmaAllocator, VulkanContext::device, VulkanContext::graphicsQueue, spheres.data(), bufferSize);
    



}

void VoxelEngine::recordFrameCommands(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame)
{	
    accelerationStructureManager.updateTLAS(commandBuffer);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline);
    
    // Bind descriptor sets
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipelineLayout, 
                           0, 1, &descriptorSetsRT[currentFrame], 0, nullptr);


	vkCmdPushConstants(commandBuffer, rtPipelineLayout, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
		0, sizeof(constants), &constants);
    
    // Set up shader binding tables
    VkStridedDeviceAddressRegionKHR raygenRegion{};
    raygenRegion.deviceAddress = raygenShaderBindingTable.deviceAddress;
    raygenRegion.stride = VulkanContext::rtProperties.shaderGroupHandleSize;
    raygenRegion.size = VulkanContext::rtProperties.shaderGroupHandleSize;
    
    VkStridedDeviceAddressRegionKHR missRegion{};
    missRegion.deviceAddress = missShaderBindingTable.deviceAddress;
    missRegion.stride = VulkanContext::rtProperties.shaderGroupHandleSize;
    missRegion.size = VulkanContext::rtProperties.shaderGroupHandleSize;
    
    VkStridedDeviceAddressRegionKHR hitRegion{};
    hitRegion.deviceAddress = closestHitShaderBindingTable.deviceAddress;
    hitRegion.stride = VulkanContext::rtProperties.shaderGroupHandleSize;
    hitRegion.size = VulkanContext::rtProperties.shaderGroupHandleSize;
    
    VkStridedDeviceAddressRegionKHR callableRegion{};
    
    VulkanContext::vkCmdTraceRaysKHR(
        commandBuffer,
        &raygenRegion, 
        &missRegion, 
        &hitRegion, 
        &callableRegion,
        swapChainExtent.width, 
        swapChainExtent.height, 
        1
    );

	VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.image = outputImage.image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    
    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    // Transition swapchain image to TRANSFER_DST_OPTIMAL
    VkImageMemoryBarrier swapchainBarrier{};
    swapchainBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    swapchainBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    swapchainBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    swapchainBarrier.image = swapChainImages[imageIndex];
    swapchainBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    swapchainBarrier.srcAccessMask = 0;
    swapchainBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    
    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &swapchainBarrier
    );
	
	
    VkImageBlit blitRegion{};
    blitRegion.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    blitRegion.srcOffsets[0] = {0, 0, 0};
    blitRegion.srcOffsets[1] = {(int)swapChainExtent.width, (int)swapChainExtent.height, 1};
    blitRegion.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    blitRegion.dstOffsets[0] = {0, 0, 0};
    blitRegion.dstOffsets[1] = {(int)swapChainExtent.width, (int)swapChainExtent.height, 1};

    vkCmdBlitImage(
        commandBuffer,
        outputImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        swapChainImages[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &blitRegion,
        VK_FILTER_LINEAR
    );


	// Transition swapchain image to PRESENT_SRC_KHR
    VkImageMemoryBarrier presentBarrier{};
    presentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    presentBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    presentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    presentBarrier.image = swapChainImages[imageIndex];
    presentBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    presentBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    presentBarrier.dstAccessMask = 0;
    
    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &presentBarrier
    );

    // Transition outputImage back to GENERAL for next frame
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    
    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

	VkImageMemoryBarrier imguiBarrier{};
    imguiBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imguiBarrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    imguiBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    imguiBarrier.image = swapChainImages[imageIndex];
    imguiBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    imguiBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imguiBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    
    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0, 0, nullptr, 0, nullptr, 1, &imguiBarrier
    );

    // Render ImGui
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = imguiHandler.imguiRenderPass;
    renderPassInfo.framebuffer = imguiHandler.imguiFramebuffers[imageIndex];
    renderPassInfo.renderArea.extent = swapChainExtent;
    renderPassInfo.clearValueCount = 1;
    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
    vkCmdEndRenderPass(commandBuffer);
}

void VoxelEngine::updateUniformBuffersRT()
{
	uniformData.view_inverse = fpsCamera.getInverseViewMatrix();
	uniformData.proj_inverse = fpsCamera.getInverseProjectionMatrix();
	uniformData.position = fpsCamera.getPosition();

	for (auto& ubo : uniformBuffersRT)
	{
		if (ubo.type == ManagedBuffer::BufferType::HostVisible)
		{
			void* mapped;
			vmaMapMemory(VulkanContext::vmaAllocator, ubo.allocation, &mapped);
			memcpy(mapped, &uniformData, sizeof(UniformData));
			vmaUnmapMemory(VulkanContext::vmaAllocator, ubo.allocation);
		} 
		else 
		{
			LOG_ERROR("Trying to update non-host-visible uniform buffer");
		}
	}
}

void VoxelEngine::createCamera()
{
	fpsCamera = FirstPersonCamera({ 0.0f, 0.0f, 0.0f }, FOV, static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height));
}

void VoxelEngine::handleInput(GLFWwindow* window, float dt)
{
    // Movement speed with shift for fast movement
    float moveSpeed = MOVEMENT_SENS;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        moveSpeed *= 5.0f;  // Move faster when holding shift
    
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
	{
		fpsCamera.moveForward(dt * moveSpeed);
	}
		
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
	{
		fpsCamera.moveBackward(dt * moveSpeed);
	}
    
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
	{
		fpsCamera.moveLeft(dt * moveSpeed);
	}

	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
	{
		fpsCamera.moveRight(dt * moveSpeed);
	}
        
	if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
	{
		fpsCamera.moveUp(dt * moveSpeed);
	}
	if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
	{
		fpsCamera.moveDown(dt * moveSpeed);
	}
        
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

void VoxelEngine::recreateSwapChain() 
{
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(VulkanContext::device);  

	if (!commandBuffers.empty()) 
	{
        vkFreeCommandBuffers(VulkanContext::device, VulkanContext::commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
    }
    createCommandBuffers();


    cleanupSwapChain();
	cleanupSyncObjects();

    createSwapChain();
    createImageViews();
    createStorageImages();
	createSyncObjects();
    createCommandBuffers(); 

	imguiHandler.destroyFramebuffers(VulkanContext::device);
    imguiHandler.createImguiFramebuffers(VulkanContext::device, swapChainImageViews, swapChainExtent);

	fpsCamera.setAspectRatio(static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height));
    updateDescriptorSetsRT();

	currentFrame = 0;
}

void VoxelEngine::drawFrameRT() 
{
    // Wait for the previous frame to complete
    vkWaitForFences(VulkanContext::device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
    
	
    // Update uniform buffers for the current frame BEFORE recording commands
    updateUniformBuffersRT();
    
    // Acquire an image from the swap chain
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(
        VulkanContext::device, 
        swapChain, 
        UINT64_MAX, 
        imageAvailableSemaphores[currentFrame], 
        VK_NULL_HANDLE, 
        &imageIndex
    );
    
    // Check if swap chain needs recreation
    if (result == VK_ERROR_OUT_OF_DATE_KHR || framebufferResized) {
        framebufferResized = false;
        recreateSwapChain();
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        LOG_ERROR("failed to acquire swap chain image!");
    }
    
    // Check if a previous frame is using this image (i.e., there is its fence to wait on)
    if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(VulkanContext::device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }
    // Mark the image as now being in use by this frame
    imagesInFlight[imageIndex] = inFlightFences[currentFrame];
    
    // Reset the fence for this frame
    vkResetFences(VulkanContext::device, 1, &inFlightFences[currentFrame]);
    
    // Reset and begin recording the command buffer
    vkResetCommandBuffer(commandBuffers[currentFrame], 0);
    
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    VK_ERROR_CHECK(vkBeginCommandBuffer(commandBuffers[currentFrame], &beginInfo));
    

    recordFrameCommands(commandBuffers[currentFrame], imageIndex, currentFrame);

    VK_ERROR_CHECK(vkEndCommandBuffer(commandBuffers[currentFrame]));
    
    
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    
    VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[currentFrame];
    
    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    VK_ERROR_CHECK(vkQueueSubmit(VulkanContext::graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]));

    
    // Present the image
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    
    VkSwapchainKHR swapChains[] = {swapChain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;
    
    result = vkQueuePresentKHR(VulkanContext::presentQueue, &presentInfo);


    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
        framebufferResized = false;
        recreateSwapChain();
    } else if (result != VK_SUCCESS) {
        LOG_ERROR("failed to present swap chain image!");
    }
    
    // Advance to the next frame
    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;    
}

void VoxelEngine::cleanupSyncObjects() 
{
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(VulkanContext::device, imageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(VulkanContext::device, renderFinishedSemaphores[i], nullptr);
        vkDestroyFence(VulkanContext::device, inFlightFences[i], nullptr);
    }
	
	imageAvailableSemaphores.clear();
    renderFinishedSemaphores.clear();
    inFlightFences.clear();
	imagesInFlight.clear();
}


void VoxelEngine::cleanup()
{
	cleanupRayTracing();
	cleanupSwapChain();
	cleanupSyncObjects();
	
	if (descriptorPool != VK_NULL_HANDLE) vkDestroyDescriptorPool(VulkanContext::device, descriptorPool, nullptr);

	imguiHandler.destroy(VulkanContext::device);

	VulkanContext::Cleanup();

	glfwDestroyWindow(window);
	glfwTerminate();
}

void VoxelEngine::cleanupSwapChain()
{
	for (size_t i = 0; i < swapChainImageViews.size(); ++i)
	{
		vkDestroyImageView(VulkanContext::device, swapChainImageViews[i], nullptr);
	}
	swapChainImageViews.clear();
	vkDestroySwapchainKHR(VulkanContext::device, swapChain, nullptr);

	if (!commandBuffers.empty()) 
	{
        vkFreeCommandBuffers(VulkanContext::device, VulkanContext::commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
        commandBuffers.clear();
    }
}


void VoxelEngine::cleanupRayTracing()
{
	if (rtPipeline != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(VulkanContext::device, rtPipeline, nullptr);
	}

	if (rtPipelineLayout != VK_NULL_HANDLE)
	{
		vkDestroyPipelineLayout(VulkanContext::device, rtPipelineLayout, nullptr);
	}

	if (descriptorSetLayoutRT != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorSetLayout(VulkanContext::device, descriptorSetLayoutRT, nullptr);
	}

    accelerationStructureManager.destroy();
	

	raygenShaderBindingTable.destroy(VulkanContext::vmaAllocator);
	missShaderBindingTable.destroy(VulkanContext::vmaAllocator);
	closestHitShaderBindingTable.destroy(VulkanContext::vmaAllocator);

	outputImage.destroy();
	debugImage.destroy();

	for (auto& ubo : uniformBuffersRT)
	{
		ubo.destroy(VulkanContext::vmaAllocator);
	}
	
	sphereBuffer.destroy(VulkanContext::vmaAllocator);
}
