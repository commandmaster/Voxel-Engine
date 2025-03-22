#pragma once


#define ALLOC_VMA
#include "vma/vk_mem_alloc.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <shaderc/shaderc.hpp>
#include <glm\glm.hpp>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

#include "Camera.h"
#include "Timer.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstdlib>
#include <stdint.h>
#include <cstdint>
#include <cstring>
#include <string>
#include <set>
#include <vector>
#include <optional>
#include <limits>
#include <algorithm> 
#include <array>

namespace Shader
{
    std::string readShaderFile(const std::string& path);

    std::vector<uint32_t> compileGLSLToSPIRV(const std::string& source, shaderc_shader_kind kind, const char* shaderName = "shader.glsl");

   VkShaderModule createShaderModule(VkDevice device, const std::vector<uint32_t>& spirv);
}

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation",
};

const std::vector<const char*> deviceExtensions = {
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

#ifdef NDEBUG
	constexpr bool enableValidationLayers = false;
#else
	constexpr bool enableValidationLayers = true;
#endif

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);

struct QueueFamilyIndices 
{
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    inline bool isComplete() const { return graphicsFamily.has_value() && presentFamily.has_value(); }
};

struct SwapChainSupportDetails 
{
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};


struct Sphere 
{
    glm::vec4 positionRadius; // xyz=position, w=radius
};

struct ScratchBuffer
{
    uint64_t deviceAddress = 0;
    VkBuffer handle = VK_NULL_HANDLE;
    VmaAllocation allocation;

    static void createScratchBuffer(VmaAllocator allocator, VkDevice device, VkDeviceSize size, ScratchBuffer& scratchBuffer);

    static void destroyScratchBuffer(VmaAllocator allocator, ScratchBuffer& scratchBuffer);
};

class ManagedBuffer 
{
public:
    enum class BufferType 
    {
        HostVisible,
        DeviceLocal
    };

    VkBuffer handle = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
    uint64_t deviceAddress = 0;
    BufferType type = BufferType::DeviceLocal;

    ManagedBuffer() = default;

    ~ManagedBuffer() noexcept(false) 
    {
        if (!_isDestroyed) 
        {
            throw std::runtime_error("Buffer destroyed implicitly! Call destroy() explicitly first.");
        }
    }

    // Explicit creation/destruction
    void create(
        VmaAllocator allocator,
        VkDevice device,
        VkDeviceSize bufferSize,
        VkBufferUsageFlags usage,
        BufferType bufferType
    );

    void destroy(VmaAllocator allocator) 
    {
        if (handle != VK_NULL_HANDLE) {
            vmaDestroyBuffer(allocator, handle, allocation);
            handle = VK_NULL_HANDLE;
            allocation = VK_NULL_HANDLE;
            size = 0;
            deviceAddress = 0;
        }
        _isDestroyed = true;
    }

    // Data transfer operations
    void uploadData(
        VmaAllocator allocator,
        VkDevice device,
        VkCommandBuffer commandBuffer,
        VkQueue queue,
        const void* data,
        VkDeviceSize dataSize,
        VkDeviceSize offset = 0
    );

    void updateData(
        VmaAllocator allocator,
        const void* data,
        VkDeviceSize dataSize,
        VkDeviceSize offset = 0
    ) 
    {
        if (type != BufferType::HostVisible)
        {
            throw std::runtime_error("Direct updates only allowed for host-visible buffers");
        }

        void* mapped;
        vmaMapMemory(allocator, allocation, &mapped);
        memcpy(static_cast<char*>(mapped) + offset, data, dataSize);
        vmaUnmapMemory(allocator, allocation);
    }

    // Move operations
    ManagedBuffer(ManagedBuffer&& other) noexcept 
    {
        *this = std::move(other);
    }

    ManagedBuffer& operator=(ManagedBuffer&& other) noexcept 
    {
        handle = other.handle;
        allocation = other.allocation;
        size = other.size;
        deviceAddress = other.deviceAddress;
        type = other.type;
        _isDestroyed = other._isDestroyed;

        other.handle = VK_NULL_HANDLE;
        other.allocation = VK_NULL_HANDLE;
        other.size = 0;
        other.deviceAddress = 0;
        other._isDestroyed = true;
        
        return *this;
    }

private:
    bool _isDestroyed = true;
};




class StorageImage
{
public:
    VkImage        image = VK_NULL_HANDLE;
    VkImageView    view = VK_NULL_HANDLE;
    VmaAllocation  allocation = VK_NULL_HANDLE;
    VkFormat       format;
    VkExtent3D     extent;

    VmaAllocator   allocator = VK_NULL_HANDLE;
    VkDevice       device = VK_NULL_HANDLE;

    StorageImage() = default;
    ~StorageImage() { destroy(); }

    // Prevent copy operations
    StorageImage(const StorageImage&) = delete;
    StorageImage& operator=(const StorageImage&) = delete;

    // Allow move operations
    StorageImage(StorageImage&& other) noexcept { moveFrom(std::move(other)); }
    StorageImage& operator=(StorageImage&& other) noexcept { moveFrom(std::move(other)); return *this; }

    void create(VmaAllocator alloc, VkDevice dev, VkExtent3D imgExtent, VkFormat imgFormat, VkImageUsageFlags usage)
    {
        allocator = alloc;  
        device = dev;        

        extent = imgExtent;
        format = imgFormat;

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent = extent;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = imageInfo.usage = usage | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        if (vmaCreateImage(allocator, &imageInfo, &allocInfo, &image, &allocation, nullptr) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create storage image!");
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &viewInfo, nullptr, &view) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create image view!");
        }
    }

    void destroy()
    {
        if ((view != VK_NULL_HANDLE || image != VK_NULL_HANDLE) && (device == VK_NULL_HANDLE || allocator == VK_NULL_HANDLE))        
        {
            throw std::runtime_error("Logical device is NULL HANDLE while trying to delete resources!");
        }

        if (view != VK_NULL_HANDLE)
        {
            vkDestroyImageView(device, view, nullptr);
            view = VK_NULL_HANDLE;
        }

        if (image != VK_NULL_HANDLE)
        {
            vmaDestroyImage(allocator, image, allocation);
            image = VK_NULL_HANDLE;
            allocation = VK_NULL_HANDLE;
        }
    }

private:
    void moveFrom(StorageImage&& other)
    {
        image = other.image;
        view = other.view;
        allocation = other.allocation;
        format = other.format;
        extent = other.extent;
        allocator = other.allocator;
        device = other.device;

        other.image = VK_NULL_HANDLE;
        other.view = VK_NULL_HANDLE;
        other.allocation = VK_NULL_HANDLE;
        other.allocator = VK_NULL_HANDLE;
        other.device = VK_NULL_HANDLE;
    }
};




struct AccelerationStructure
{
    VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
    uint64_t deviceAddress;
    ManagedBuffer buffer;
};





class VoxelEngine 
{
public:
    void run() 
    {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

public:
	static PFN_vkQueueSubmit2KHR vkQueueSubmit2KHR;
    
    static PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
	static PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
	static PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
	static PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
	static PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;

    static PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR;
	static PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR;
	static PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;
  
    static PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;

private:
    struct UniformData
    {
        glm::mat4 view_inverse;
        glm::mat4 proj_inverse;
    };

	struct Constants 
    {
	    alignas(16) glm::vec3 lightDir = glm::normalize(glm::vec3(1.0f, 1.0f, 1.0f));
		alignas(16) glm::vec3 lightColor = glm::vec3(1.0f, 1.0f, 1.0f);
		alignas(16) glm::vec3 ambientColor = glm::vec3(0.1f, 0.1f, 0.1f);	
    } constants;

    struct ImguiHandler
    {
        VkRenderPass imguiRenderPass;
        std::vector<VkFramebuffer> imguiFramebuffers;
        VkDescriptorPool imguiDescriptorPool = VK_NULL_HANDLE;


        void initImgui(GLFWwindow* window, VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice logicalDevice, uint32_t queueFamilyIndex, VkQueue queue, uint32_t swapChainImagesCount, std::vector<VkImageView>& swapChainImageViews, VkExtent2D swapChainExtent, VkFormat swapChainImageFormat)
        {
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO(); (void)io;

            ImGui::StyleColorsDark();

            ImGui_ImplGlfw_InitForVulkan(window, true);
            ImGui_ImplVulkan_InitInfo initInfo = {};
            initInfo.Instance = instance;
            initInfo.PhysicalDevice = physicalDevice;
            initInfo.Device = logicalDevice;
            initInfo.QueueFamily = queueFamilyIndex;
            initInfo.Queue = queue;
            initInfo.PipelineCache = VK_NULL_HANDLE;

            initDescriptorPool(logicalDevice);
            initInfo.DescriptorPool = imguiDescriptorPool;

            initInfo.Allocator = nullptr;
            initInfo.ImageCount = swapChainImagesCount;
            initInfo.MinImageCount = swapChainImagesCount;
            initInfo.CheckVkResultFn = ImguiHandler::IMGUI_ERROR_HANDLER;

            createImguiRenderPass(logicalDevice, swapChainImageFormat);
            createImguiFramebuffers(logicalDevice, swapChainImageViews, swapChainExtent);

            initInfo.RenderPass = imguiRenderPass;

            ImGui_ImplVulkan_Init(&initInfo);
        }

		void createImguiFramebuffers(VkDevice device, std::vector<VkImageView>& swapChainImageViews, VkExtent2D swapChainExtent) 
        {
			imguiFramebuffers.resize(swapChainImageViews.size());
			for (size_t i = 0; i < swapChainImageViews.size(); i++) 
            {
				VkImageView attachments[] = { swapChainImageViews[i] };
				VkFramebufferCreateInfo framebufferInfo{};
				framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
				framebufferInfo.renderPass = imguiRenderPass;
				framebufferInfo.attachmentCount = 1;
				framebufferInfo.pAttachments = attachments;
				framebufferInfo.width = swapChainExtent.width;
				framebufferInfo.height = swapChainExtent.height;
				framebufferInfo.layers = 1;

                auto res = vkCreateFramebuffer(device, &framebufferInfo, nullptr, &imguiFramebuffers[i]);
                IMGUI_ERROR_HANDLER(res);
			}
		}

        void destroyFramebuffers(VkDevice device) 
        {
			for (auto& fb : imguiFramebuffers) vkDestroyFramebuffer(device, fb, nullptr);
			imguiFramebuffers.clear();
		}

        void destroy(VkDevice device)
        {

            vkDestroyRenderPass(device, imguiRenderPass, nullptr);

            destroyFramebuffers(device);

            ImGui_ImplVulkan_Shutdown();
			ImGui_ImplGlfw_Shutdown();
			ImGui::DestroyContext();

            vkDestroyDescriptorPool(device, imguiDescriptorPool, nullptr);
            imguiDescriptorPool = VK_NULL_HANDLE;
        }

        ~ImguiHandler()
        {
            if (imguiDescriptorPool != VK_NULL_HANDLE)
            {
                throw std::runtime_error("Imgui Handler was not destroyed prior to going out of scope. Call ImguiHandler.destroy() to explicitly destroy the handler");
            }

        }

    private:
        void initDescriptorPool(VkDevice device)
        {
            VkDescriptorPoolSize poolSizes[] =
			{
				{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
				{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
				{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
				{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
				{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
				{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
				{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
			};

            VkDescriptorPoolCreateInfo poolInfo{};
            poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
            poolInfo.maxSets = 0;
            for (VkDescriptorPoolSize& poolSize : poolSizes)
            {
                poolInfo.maxSets += poolSize.descriptorCount;
            }
            
            poolInfo.poolSizeCount = static_cast<uint32_t>(IM_ARRAYSIZE(poolSizes));
            poolInfo.pPoolSizes = poolSizes;
            if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &imguiDescriptorPool) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create IMGUI descriptor pool");
            }
        }

        void createImguiRenderPass(VkDevice device, VkFormat swapChainImageFormat) 
        {
			VkAttachmentDescription colorAttachment{};
			colorAttachment.format = swapChainImageFormat;
			colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
			colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			colorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

			VkAttachmentReference colorAttachmentRef{};
			colorAttachmentRef.attachment = 0;
			colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			VkSubpassDescription subpass{};
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass.colorAttachmentCount = 1;
			subpass.pColorAttachments = &colorAttachmentRef;

			VkSubpassDependency dependency{};
			dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
			dependency.dstSubpass = 0;
			dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependency.srcAccessMask = 0;
			dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

			VkRenderPassCreateInfo renderPassInfo{};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			renderPassInfo.attachmentCount = 1;
			renderPassInfo.pAttachments = &colorAttachment;
			renderPassInfo.subpassCount = 1;
			renderPassInfo.pSubpasses = &subpass;
			renderPassInfo.dependencyCount = 1;
			renderPassInfo.pDependencies = &dependency;

			auto res = vkCreateRenderPass(device, &renderPassInfo, nullptr, &imguiRenderPass);
            IMGUI_ERROR_HANDLER(res);
		}

        
        static void IMGUI_ERROR_HANDLER(VkResult err)
        {
            if (err != VK_SUCCESS)
            {
                throw std::runtime_error("IMGUI vulkan error, enum code: " + std::to_string((int)err));
            }
        }

    } imguiHandler;

    
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback
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

    const uint32_t WIDTH = 800;
    const uint32_t HEIGHT = 600;

    const uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    const float MOVEMENT_SENS = 0.003f;

    uint32_t currentFrame = 0;

    const std::string windowName = "Vulkan Example";

    GLFWwindow* window;

    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;

    VkQueue graphicsQueue;
    VkQueue presentQueue;
    VkSurfaceKHR surface;

    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkImageLayout> swapChainImageLayouts; 

    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;

    std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::vector<VkFence> inFlightFences;
    std::vector<VkFence> imagesInFlight;


    bool framebufferResized = false;

    
    UniformData uniformData;
    
    std::vector<ManagedBuffer> uniformBuffersRT;

    const float FOV = 60.0f;
    Camera camera;

    double lastMouseX = 0.0;
    double lastMouseY = 0.0;
    bool firstMouse = true;

    StorageImage outputImage;
    StorageImage accumulationImage;

    VmaAllocator vmaAllocator = VK_NULL_HANDLE;
    
    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };

    VkPipeline rtPipeline = VK_NULL_HANDLE;
    VkPipelineLayout rtPipelineLayout = VK_NULL_HANDLE;

	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptorSetsRT;
	VkDescriptorSetLayout descriptorSetLayoutRT = VK_NULL_HANDLE;

    AccelerationStructure bottomLevelAccelerationStructure;
    AccelerationStructure topLevelAccelerationStructure;

    std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups{};

    ManagedBuffer raygenShaderBindingTable;
    ManagedBuffer missShaderBindingTable;
    ManagedBuffer closestHitShaderBindingTable;

    std::vector<Sphere> spheres = { { {0, 0, 0, 1.0f} }, { {2, 0, 0, 2.0f} } };
    ManagedBuffer sphereBuffer;
    
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
    {
        auto app = reinterpret_cast<VoxelEngine*>(glfwGetWindowUserPointer(window));
        app->framebufferResized = true;
	}

    static void mouseCallback(GLFWwindow* window, double xpos, double ypos);

    void initWindow();

    std::vector<const char*> getRequiredExtensions();

    bool checkValidationLayerSupport();

    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

    void setupDebugMessenger();

    void createInstance();
    
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

    bool checkDeviceExtensionSupport(VkPhysicalDevice device);

	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

	bool isDeviceSuitable(VkPhysicalDevice device);

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

    void pickPhysicalDevice();
    
    void createLogicalDevice();

    void createSurface();

    void createSwapChain();

    void createImageViews();

    void createCommandPool();


    VkCommandBuffer createCommandBuffer(VkDevice device, VkCommandPool commandPool, VkCommandBufferLevel level, bool singleUse);

    void flushCommandBuffer(VkDevice device, VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool commandPool, bool free);
    
    void createCommandBuffers();


    void initVMA();

    uint64_t getBufferDeviceAddress(VkBuffer buffer);

    __forceinline ScratchBuffer createScratchBuffer(VkDeviceSize size) const
	{
		ScratchBuffer scratchBuffer{};
        ScratchBuffer::createScratchBuffer(vmaAllocator, device, size, scratchBuffer);
	}

	__forceinline void deleteScratchBuffer(ScratchBuffer& scratchBuffer) const
	{
		ScratchBuffer::destroyScratchBuffer(vmaAllocator, scratchBuffer);
	}

    __forceinline uint32_t alignedSize(uint32_t value, uint32_t alignment)
	{
		return (value + alignment - 1) & ~(alignment - 1);
	}

    void createStorageImages();

    void createBLAS();

    void createTLAS();

    void deleteAS(AccelerationStructure& accelerationStructure);

    void createShaderBindingTables();

    void createDescriptorSetsRT();

    void updateDescriptorSetsRT();

    void updateDescriptorSetRT(uint32_t index);

    void createRayTracingPipeline();

    void createUniformBuffers();

    void createSyncObjects();

    void createSphereBuffer();

    void recordCommandBufferRT(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame);

    void updateUniformBuffersRT();

    void createCamera();

    void handleInput(GLFWwindow* window);

    
    void initVulkan() 
    {
        createInstance();
		setupDebugMessenger();
		createSurface();
		pickPhysicalDevice();
		createLogicalDevice();
		initVMA();
		createCommandPool();
		createSwapChain();
		createImageViews();
		createSyncObjects(); 
		createCommandBuffers();
		createStorageImages();
		createCamera();
		createSphereBuffer();
		createUniformBuffers();
		createBLAS();
		createTLAS();
		createRayTracingPipeline();
		createShaderBindingTables();
		createDescriptorSetsRT();        

        auto queueFamilyIndicies = findQueueFamilies(physicalDevice);
        imguiHandler.initImgui(window, instance, physicalDevice, device, queueFamilyIndicies.graphicsFamily.value(), graphicsQueue, static_cast<uint32_t>(swapChainImages.size()), swapChainImageViews, swapChainExtent, swapChainImageFormat);
    }

    void recreateSwapChain();

    void drawFrameRT();

    void cleanupSyncObjects();

    void mainLoop() 
    {
		double accumulation = 0;
		std::vector<double> frameTimes;
		Timer frameTimer;
		
		constexpr int MAX_SAMPLES = 100; // Store the last 100 frames for averaging
		frameTimes.reserve(MAX_SAMPLES);
		
		int frameCount = 0;
		
		while (!glfwWindowShouldClose(window)) {
			frameTimer.start();
			
			glfwPollEvents();
			handleInput(window);
			
			ImGui_ImplVulkan_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();
			
			if (frameTimes.size() > 0) {
				double averageFrameTime = accumulation / frameTimes.size();
				double fps = 1000000.0 / averageFrameTime; // Assuming milliseconds
				
				accumulation += frameTimes.back();
					
				ImGui::Begin("Performance Metrics");
				ImGui::Text("FPS: %.1f", fps);
				ImGui::Text("Frame Time: %.2f ms", averageFrameTime);
				ImGui::Text("Frame Count: %d", frameCount);
				ImGui::End();
			}
			
			ImGui::ShowDemoWindow();
			ImGui::Render();
			drawFrameRT();
			
			frameTimer.stop();
			
			double frameTime = frameTimer.elapsedTime<std::chrono::microseconds>();
			frameCount++;
			
			frameTimes.push_back(frameTime);
			
			if (frameTimes.size() > MAX_SAMPLES) {
				accumulation -= frameTimes.front();
				frameTimes.erase(frameTimes.begin());
			}
		}
		
		vkDeviceWaitIdle(device);
	}

    void cleanup();

    void cleanupSwapChain();

    void cleanupRayTracing();



};

