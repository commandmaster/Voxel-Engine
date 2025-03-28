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
#include "PerformanceTimer.hpp"

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
#include <random>

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
        initVulkan();
        mainLoop();
        cleanup();
    }

    VoxelEngine(VoxelEngine const&) = delete;
    void operator=(VoxelEngine const&) = delete;
    
private:
    struct UniformData
    {
        glm::mat4 view_inverse;
        glm::mat4 proj_inverse;
        glm::vec3 position;
    };

	struct Constants 
    {
	    alignas(16) glm::vec3 lightDir = glm::normalize(glm::vec3(1.0f, -1.0f, -1.0f));
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

    
    const uint32_t WIDTH = 800;
    const uint32_t HEIGHT = 600;

    const uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    const float MOVEMENT_SENS = 22.f;

    uint32_t currentFrame = 0;

    const std::string windowName = "Vulkan Example";

    GLFWwindow* window;

    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkImageLayout> swapChainImageLayouts; 

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

    VkPipeline rtPipeline = VK_NULL_HANDLE;
    VkPipelineLayout rtPipelineLayout = VK_NULL_HANDLE;

	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptorSetsRT;
	VkDescriptorSetLayout descriptorSetLayoutRT = VK_NULL_HANDLE;

    AccelerationStructureManager accelerationStructureManager;
    uint32_t movingIndex = 0;

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
    Buffer<BufferType::DeviceLocal> sphereBuffer;
    
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
    {
        auto app = reinterpret_cast<VoxelEngine*>(glfwGetWindowUserPointer(window));
        app->framebufferResized = true;
	}

    static void mouseCallback(GLFWwindow* window, double xpos, double ypos);

    void initWindow();

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

    void createSwapChain();

    void createImageViews();

    static VkCommandBuffer createCommandBuffer(VkDevice device, VkCommandPool commandPool, VkCommandBufferLevel level, bool singleUse);

    static void flushCommandBuffer(VkDevice device, VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool commandPool, bool free);
    
    void createCommandBuffers();

    uint64_t getBufferDeviceAddress(VkBuffer buffer);

    __forceinline uint32_t alignedSize(uint32_t value, uint32_t alignment)
	{
		return (value + alignment - 1) & ~(alignment - 1);
	}

    void createStorageImages();

    void createBLAS();

    void createTLAS();

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
        VulkanContext::Init(window, windowName.c_str());

		createSwapChain();
		createImageViews();
		createSyncObjects(); 
		createCommandBuffers();
		createStorageImages();
		createCamera();
		createUniformBuffers();
		createBLAS();
		createTLAS();
		createRayTracingPipeline();
		createShaderBindingTables();
		createDescriptorSetsRT();        

        auto queueFamilyIndicies = VulkanContext::FindQueueFamilies(VulkanContext::physicalDevice);
        imguiHandler.initImgui(window, VulkanContext::instance, VulkanContext::physicalDevice, VulkanContext::device, queueFamilyIndicies.graphicsFamily.value(), VulkanContext::graphicsQueue, static_cast<uint32_t>(swapChainImages.size()), swapChainImageViews, swapChainExtent, swapChainImageFormat);
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

                for (const auto& [key, value] : PerformanceTimer::getInstance().perfStats)
                {
                    ImGui::Text("%s: %.2f micro seconds", key.c_str(), value);
                }

				ImGui::End();
			}
			
			ImGui::ShowDemoWindow();
			ImGui::Render();

            float time = (float)glfwGetTime();
            VkTransformMatrixKHR transform =
            {
               1.0f, 0.0f, 0.0f, 0.0f,
			   0.0f, 1.0f, 0.0f, sin(time) * 3.f,
			   0.0f, 0.0f, 1.0f, 0.0f,
            };

            accelerationStructureManager.moveBLAS(movingIndex, transform);

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
		
		vkDeviceWaitIdle(VulkanContext::device);
	}

    void cleanup();

    void cleanupSwapChain();

    void cleanupRayTracing();
};

