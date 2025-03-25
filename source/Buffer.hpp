#pragma once

#define ALLOC_VMA
#include "vma/vk_mem_alloc.h"
#include <vulkan/vulkan.h>
#include <stdexcept>
#include <cstring>
#include <memory>
#include <vector>
#include <atomic>
#include <string>
#include <iostream>

inline std::string vkResultToString(VkResult result) 
{
    switch (result) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
        case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
        default: return "UNKNOWN_ERROR";
    }
}

enum class BufferType
{
    HostVisible,  // Optimized for fast changes and data transfer
    DeviceLocal   // Optimized for on-GPU performance
};

namespace BufferUtils
{
    inline void fastMemcpy(void* dst, const void* src, size_t size) 
    {
        if (size < 1024) 
        {
            memcpy(dst, src, size);
            return;
        }
        
        if (size < 16384 && 
            (reinterpret_cast<uintptr_t>(dst) % 16 == 0) && 
            (reinterpret_cast<uintptr_t>(src) % 16 == 0)) 
        {
            char* dstPtr = static_cast<char*>(dst);
            const char* srcPtr = static_cast<const char*>(src);
            
            size_t blocks = size / 16;
            for (size_t i = 0; i < blocks; i++) 
            {
                memcpy(dstPtr, srcPtr, 16);
                dstPtr += 16;
                srcPtr += 16;
            }
            
            size_t remaining = size % 16;
            if (remaining > 0) 
            {
                memcpy(dstPtr, srcPtr, remaining);
            }
            return;
        }
        
        memcpy(dst, src, size);
    }
    
    class FencePool
    {
    public:
        static FencePool& getInstance() 
        {
            static FencePool instance;
            return instance;
        }

        VkFence acquireFence(VkDevice device)
        {
            if (_availableFences.empty())
            {
                VkFenceCreateInfo fenceInfo{};
                fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
                
                VkFence fence = VK_NULL_HANDLE;
                if (vkCreateFence(device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) 
                {
                    throw std::runtime_error("Failed to create fence for buffer operations");
                }
                _allocatedFenceCount.fetch_add(1, std::memory_order_relaxed);
                return fence;
            }
            
            VkFence fence = VK_NULL_HANDLE;
            {
                fence = _availableFences.back();
                _availableFences.pop_back();
            }
            
            VkResult resetResult = vkResetFences(device, 1, &fence);
            if (resetResult != VK_SUCCESS) 
            {
                vkDestroyFence(device, fence, nullptr);
                throw std::runtime_error("Failed to reset fence for buffer operations");
            }
            
            return fence;
        }
        
        void releaseFence(VkFence fence)
        {
            if (fence != VK_NULL_HANDLE) 
            {
                _availableFences.push_back(fence);
            }
        }
        
        void cleanup(VkDevice device)
        {
            if (_allocatedFenceCount.load(std::memory_order_relaxed) != _availableFences.size()) 
            {
                throw std::runtime_error("Not all fences were released before cleanup!");
            }

            for (auto fence : _availableFences)
            {
                if (fence != VK_NULL_HANDLE) 
                {
                    vkDestroyFence(device, fence, nullptr);
                }
            }
            _availableFences.clear();
            _allocatedFenceCount.store(0, std::memory_order_relaxed);
        }
        
        size_t getAllocatedFenceCount() const 
        {
            return _allocatedFenceCount.load(std::memory_order_relaxed);
        }
        
    private:
        std::vector<VkFence> _availableFences;
        std::atomic<size_t> _allocatedFenceCount{0};
        
        FencePool() = default;
        ~FencePool() 
        {
            if (_allocatedFenceCount.load(std::memory_order_relaxed) > 0) 
            {
                std::cerr << "Warning: Fence pool was not properly cleaned up" << std::endl;
            }
        }
    };
}

template<BufferType Type>
class Buffer
{
public:
    VkBuffer handle = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
    uint64_t deviceAddress = 0;

    Buffer() : _isDestroyed(true), _mappedMemory(nullptr) {}

    ~Buffer() noexcept
    {
        if (!_isDestroyed && handle != VK_NULL_HANDLE) 
        {
            std::cerr << "Buffer not explicitly destroyed" << std::endl;
        }
    }

    void create(
        VmaAllocator allocator,
        VkDevice device,
        VkDeviceSize bufferSize,
        VkBufferUsageFlags usage
    );

    void destroy(VmaAllocator allocator)
    {
        if (handle != VK_NULL_HANDLE)
        {
            if (_mappedMemory != nullptr)
            {
                vmaUnmapMemory(allocator, allocation);
                _mappedMemory = nullptr;
            }
            
            vmaDestroyBuffer(allocator, handle, allocation);
            handle = VK_NULL_HANDLE;
            allocation = VK_NULL_HANDLE;
            size = 0;
            deviceAddress = 0;
        }
        _isDestroyed = true;
    }

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
    );

    // Get mapped memory pointer (only valid for HostVisible buffers)
    void* getMappedMemory() const 
    {
        return _mappedMemory;
    }

    Buffer(Buffer&& other) noexcept
    {
        *this = std::move(other);
    }

    Buffer& operator=(Buffer&& other) noexcept
    {
        if (this != &other) 
        {
            handle = other.handle;
            allocation = other.allocation;
            size = other.size;
            deviceAddress = other.deviceAddress;
            _mappedMemory = other._mappedMemory;
            _isDestroyed = other._isDestroyed;

            other.handle = VK_NULL_HANDLE;
            other.allocation = VK_NULL_HANDLE;
            other.size = 0;
            other.deviceAddress = 0;
            other._mappedMemory = nullptr;
            other._isDestroyed = true;
        }
        return *this;
    }

    // Delete copy operations
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

protected:
    bool _isDestroyed;
    void* _mappedMemory;
};

// Template specialization for HostVisible buffer type
template<>
inline void Buffer<BufferType::HostVisible>::create(
    VmaAllocator allocator,
    VkDevice device,
    VkDeviceSize bufferSize,
    VkBufferUsageFlags usage
)
{
    _isDestroyed = false;
    size = bufferSize;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    VmaAllocationInfo allocationInfo{};
    VkResult result = vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &handle, &allocation, &allocationInfo);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create host-visible buffer: " + vkResultToString(result));
    }

    _mappedMemory = allocationInfo.pMappedData;
    if (_mappedMemory == nullptr) 
    {
        result = vmaMapMemory(allocator, allocation, &_mappedMemory);
        if (result != VK_SUCCESS || _mappedMemory == nullptr) 
        {
            vmaDestroyBuffer(allocator, handle, allocation);
            handle = VK_NULL_HANDLE;
            allocation = VK_NULL_HANDLE;
            throw std::runtime_error("Failed to map host-visible buffer memory: " + vkResultToString(result));
        }
    }

    if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
    {
        VkBufferDeviceAddressInfo addressInfo{};
        addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        addressInfo.buffer = handle;
        deviceAddress = vkGetBufferDeviceAddress(device, &addressInfo);
    }
}

template<>
inline void Buffer<BufferType::HostVisible>::uploadData(
    VmaAllocator allocator,
    VkDevice device,
    VkCommandBuffer commandBuffer,
    VkQueue queue,
    const void* data,
    VkDeviceSize dataSize,
    VkDeviceSize offset
)
{
    // For host-visible buffers, directly update memory
    updateData(allocator, data, dataSize, offset);
    
    // Add a memory barrier to ensure the data is visible to the GPU
    VkBufferMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = handle;
    barrier.offset = offset;
    barrier.size = dataSize;
    
    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_HOST_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0,
        0, nullptr,
        1, &barrier,
        0, nullptr
    );
}

template<>
inline void Buffer<BufferType::HostVisible>::updateData(
    VmaAllocator allocator,
    const void* data,
    VkDeviceSize dataSize,
    VkDeviceSize offset
)
{
    if (_mappedMemory == nullptr) 
    {
        throw std::runtime_error("Buffer memory not mapped");
    }
    
    if (offset + dataSize > size) 
    {
        throw std::runtime_error("Buffer update out of bounds");
    }
    
    BufferUtils::fastMemcpy(static_cast<char*>(_mappedMemory) + offset, data, dataSize);
}

template<>
inline void Buffer<BufferType::DeviceLocal>::create(
    VmaAllocator allocator,
    VkDevice device,
    VkDeviceSize bufferSize,
    VkBufferUsageFlags usage
)
{
    _isDestroyed = false;
    size = bufferSize;
    _mappedMemory = nullptr; // Device-local buffers are not host-accessible

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    
    VkResult result = vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &handle, &allocation, nullptr);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create device-local buffer: " + vkResultToString(result));
    }

    if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
    {
        VkBufferDeviceAddressInfo addressInfo{};
        addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        addressInfo.buffer = handle;
        deviceAddress = vkGetBufferDeviceAddress(device, &addressInfo);
    }
}

template<>
inline void Buffer<BufferType::DeviceLocal>::uploadData(
    VmaAllocator allocator,
    VkDevice device,
    VkCommandBuffer commandBuffer,
    VkQueue queue,
    const void* data,
    VkDeviceSize dataSize,
    VkDeviceSize offset
)
{
    if (data == nullptr)
    {
        throw std::runtime_error("Cannot upload from null data pointer");
    }
    
    if (offset + dataSize > size)
    {
        throw std::runtime_error("Upload size exceeds buffer size");
    }
    
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VmaAllocation stagingAllocation = VK_NULL_HANDLE;
    void* stagingMappedMemory = nullptr;
    
    VkBufferCreateInfo stagingBufferInfo{};
    stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingBufferInfo.size = dataSize;
    stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VmaAllocationCreateInfo stagingAllocInfo{};
    stagingAllocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
    stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    
    VmaAllocationInfo stagingAllocationInfo{};
    VkResult result = vmaCreateBuffer(allocator, &stagingBufferInfo, &stagingAllocInfo, 
                                      &stagingBuffer, &stagingAllocation, &stagingAllocationInfo);
                                      
    if (result != VK_SUCCESS || stagingBuffer == VK_NULL_HANDLE)
    {
        throw std::runtime_error("Failed to create staging buffer: " + vkResultToString(result));
    }
    
    stagingMappedMemory = stagingAllocationInfo.pMappedData;
    if (stagingMappedMemory == nullptr)
    {
        result = vmaMapMemory(allocator, stagingAllocation, &stagingMappedMemory);
        if (result != VK_SUCCESS || stagingMappedMemory == nullptr)
        {
            vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
            throw std::runtime_error("Failed to map staging buffer memory: " + vkResultToString(result));
        }
    }
    
    BufferUtils::fastMemcpy(stagingMappedMemory, data, dataSize);
    
    if (stagingAllocationInfo.pMappedData == nullptr)
    {
        vmaUnmapMemory(allocator, stagingAllocation);
    }
    
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    if (result != VK_SUCCESS)
    {
        vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
        throw std::runtime_error("Failed to begin command buffer: " + vkResultToString(result));
    }
    
    VkBufferMemoryBarrier preCopyBarrier{};
    preCopyBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    preCopyBarrier.srcAccessMask = 0;
    preCopyBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    preCopyBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    preCopyBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    preCopyBarrier.buffer = handle;
    preCopyBarrier.offset = offset;
    preCopyBarrier.size = dataSize;
    
    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, nullptr,
        1, &preCopyBarrier,
        0, nullptr
    );
    
    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = offset;
    copyRegion.size = dataSize;
    vkCmdCopyBuffer(commandBuffer, stagingBuffer, handle, 1, &copyRegion);
    
    VkBufferMemoryBarrier postCopyBarrier{};
    postCopyBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    postCopyBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    postCopyBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    postCopyBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    postCopyBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    postCopyBarrier.buffer = handle;
    postCopyBarrier.offset = offset;
    postCopyBarrier.size = dataSize;
    
    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0,
        0, nullptr,
        1, &postCopyBarrier,
        0, nullptr
    );
    
    result = vkEndCommandBuffer(commandBuffer);
    if (result != VK_SUCCESS)
    {
        vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
        throw std::runtime_error("Failed to end command buffer: " + vkResultToString(result));
    }
    
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    
    VkFence fence = BufferUtils::FencePool::getInstance().acquireFence(device);
    
    result = vkQueueSubmit(queue, 1, &submitInfo, fence);
    if (result != VK_SUCCESS)
    {
        vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
        BufferUtils::FencePool::getInstance().releaseFence(fence);
        throw std::runtime_error("Failed to submit queue: " + vkResultToString(result));
    }
    
    result = vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
    
    BufferUtils::FencePool::getInstance().releaseFence(fence);
    
    if (result != VK_SUCCESS)
    {
        vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
        throw std::runtime_error("Failed to wait for fence: " + vkResultToString(result));
    }
    
    vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
}

template<>
inline void Buffer<BufferType::DeviceLocal>::updateData(
    VmaAllocator allocator,
    const void* data,
    VkDeviceSize dataSize,
    VkDeviceSize offset
)
{
    throw std::runtime_error("Direct updates not supported for device-local buffers. Use uploadData() instead.");
}

inline void cleanupBufferResources(VkDevice device)
{
    BufferUtils::FencePool::getInstance().cleanup(device);
} 