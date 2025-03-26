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

#include "VulkanContext.hpp"

#include "FirstPersonCamera.h"
#include "Timer.h"
#include "MemoryClasses.hpp"
#include "AccelerationStructure.h"
#include "Buffer.hpp"

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

#ifdef NDEBUG
#pragma comment(lib, "shaderc_combined.lib")
#else
#pragma comment(lib, "shaderc_combinedd.lib")
#endif

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

class VoxelEngine 
{
public:
    VoxelEngine() = default;

    static VoxelEngine& getInstance()
    {
        static VoxelEngine instance;
        return instance;
    }

    void run() 
    {
        initWindow();
        VulkanContext::Init(window, windowName.c_str());
        initVulkan();
        mainLoop();
        cleanup();
    }

    VoxelEngine(VoxelEngine const&) = delete;
    void operator=(VoxelEngine const&) = delete;
    

public:
	static PFN_vkQueueSubmit2KHR vkQueueSubmit2KHR;
    
    static PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
	static PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
	static PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
	static PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
    static PFN_vkBuildAccelerationStructuresKHR vkBuildAccelerationStructuresKHR;

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
        glm::vec3 position;
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

        void destroy(VkDevice device){

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

    const float MOVEMENT_SENS = 1.f;

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
    FirstPersonCamera fpsCamera;

    double lastMouseX = 0.0;
    double lastMouseY = 0.0;
    bool firstMouse = true;

    StorageImage outputImage;
    StorageImage debugImage;

    VmaAllocator vmaAllocator = VK_NULL_HANDLE;
    
    VulkanFeatureChain featureChain;
    VkPhysicalDeviceAccelerationStructureFeaturesKHR* accelerationStructureFeatures = nullptr;
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };

    VkPipeline rtPipeline = VK_NULL_HANDLE;
    VkPipelineLayout rtPipelineLayout = VK_NULL_HANDLE;

	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptorSetsRT;
	VkDescriptorSetLayout descriptorSetLayoutRT = VK_NULL_HANDLE;

    struct AccelerationStructure
    {
        VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
        uint64_t deviceAddress;
        Buffer<BufferType::DeviceLocal> buffer;
    };

    AccelerationStructure bottomLevelAccelerationStructure;
    AccelerationStructure topLevelAccelerationStructure;

    std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups{};

    ManagedBuffer raygenShaderBindingTable;
    ManagedBuffer missShaderBindingTable;
    ManagedBuffer closestHitShaderBindingTable;

    std::vector<Sphere> spheres = { 
		{ {0, 0, 0, 1.0f} }, 
		{ {2, 0, 0, 2.0f} }, 
		{ {-3, 1, 2, 1.5f} }, 
		{ {4, -2, 1, 2.5f} }, 
		{ {0, 3, -4, 1.2f} }, 
		{ {-1, -1, -1, 0.8f} }, 
		{ {5, 5, 5, 3.0f} } 
	};    
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

    void recordFrameCommands(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame);

    void updateUniformBuffersRT();

    void createCamera();

    void handleInput(GLFWwindow* window, float dt);

    
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

        float dt = 0;
		
		while (!glfwWindowShouldClose(window)) {
			frameTimer.start();
			
			glfwPollEvents();

            if (!frameTimes.empty()) dt =  frameTimes.back() * 0.000001f;

			handleInput(window, dt);
			
			ImGui_ImplVulkan_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();
			
			if (frameTimes.size() > 0) {
				double averageFrameTime = accumulation / frameTimes.size();
				double fps = 1000000.0 / averageFrameTime; // Assuming milliseconds
				
				accumulation += frameTimes.back();
					
				ImGui::Begin("Performance Metrics");
				ImGui::Text("FPS: %.1f", fps);
				ImGui::Text("Frame Time: %.2f micro seconds", averageFrameTime);
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

