#pragma once

#define ALLOC_VMA
#include "vma/vk_mem_alloc.h"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <stdexcept>
#include <cstring>


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