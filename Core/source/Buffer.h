#pragma once

#define ALLOC_VMA
#include "vk_mem_alloc.h"
#include <vulkan/vulkan.h>
#include <stdexcept>
#include <cstring>
#include <memory>
#include <vector>
#include <atomic>
#include <mutex>
#include <string>
#include <iostream>

#include "DebugUtils.h"

struct ScratchBuffer
{
    uint64_t deviceAddress = 0;
    VkBuffer handle = VK_NULL_HANDLE;
    VmaAllocation allocation;
    PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR;

    void createScratchBuffer(VmaAllocator allocator, VkDevice device, VkDeviceSize size, PFN_vkGetBufferDeviceAddressKHR pfn_vkGetBufferDeviceAddressKHR);

    void destroyScratchBuffer(VmaAllocator allocator);
};

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
            std::lock_guard<std::mutex> lock(_mutex);
            
            if (_availableFences.empty())
            {
                VkFenceCreateInfo fenceInfo{};
                fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
                
                VkFence fence = VK_NULL_HANDLE;
                if (vkCreateFence(device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) 
                {
                    VK_ERROR_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &fence));
                }
                _allocatedFenceCount.fetch_add(1, std::memory_order_relaxed);
                return fence;
            }
            
            VkFence fence = _availableFences.back();
            _availableFences.pop_back();
            
            VkResult resetResult = vkResetFences(device, 1, &fence);
            if (resetResult != VK_SUCCESS) 
            {
                vkDestroyFence(device, fence, nullptr);
                #ifdef LOG_ERROR_MODE
                throw std::runtime_error("Failed to reset fence for buffer operations");
                #endif
            }
            
            return fence;
        }
        
        void releaseFence(VkFence fence)
        {
            if (fence != VK_NULL_HANDLE) 
            {
                std::lock_guard<std::mutex> lock(_mutex);
                _availableFences.push_back(fence);
            }
        }
        
        void cleanup(VkDevice device)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            
            if (_allocatedFenceCount.load(std::memory_order_relaxed) != _availableFences.size()) 
            {
                LOG_ERROR("Not all fences were released before cleanup!");
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
        std::mutex _mutex;
        
        FencePool() = default;
        ~FencePool() 
        {
            if (_allocatedFenceCount.load(std::memory_order_relaxed) > 0) 
            {
                LOG_ERROR("Warning: Fence pool was not properly cleaned up");
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
    PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR;

    Buffer() : _isDestroyed(false), _mappedMemory(nullptr), vkGetBufferDeviceAddressKHR(nullptr) {}

    ~Buffer() noexcept
    {
        if (!_isDestroyed && handle != VK_NULL_HANDLE) 
        {
            LOG_ERROR("Buffer not explicitly destroyed");
        }
    }

    void create(
        VmaAllocator allocator,
        VkDevice device,
        VkDeviceSize bufferSize,
        VkBufferUsageFlags usage,
        PFN_vkGetBufferDeviceAddressKHR pfn_vkGetBufferDeviceAddressKHR,
        bool isLargeAllocation = false
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
        VkQueue queue,
        const void* data,
        VkDeviceSize dataSize,
        VkDeviceSize offset = 0
    );

    void uploadDataDeffered(
        VkCommandBuffer cmd,
        VmaAllocator allocator,
        VkDevice device,
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
            vkGetBufferDeviceAddressKHR = other.vkGetBufferDeviceAddressKHR;

            other.handle = VK_NULL_HANDLE;
            other.allocation = VK_NULL_HANDLE;
            other.size = 0;
            other.deviceAddress = 0;
            other._mappedMemory = nullptr;
            other._isDestroyed = true;
            other.vkGetBufferDeviceAddressKHR = nullptr;
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
    VkBufferUsageFlags usage,
	PFN_vkGetBufferDeviceAddressKHR pfn_vkGetBufferDeviceAddressKHR,
    bool isLargeAllocation
)
{
    vkGetBufferDeviceAddressKHR = pfn_vkGetBufferDeviceAddressKHR;

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
        LOG_ERROR("Failed to create host-visible buffer: " + vkResultToString(result));
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
            LOG_ERROR("Failed to map host-visible buffer memory: " + vkResultToString(result));
        }
    }

    if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
    {
        VkBufferDeviceAddressInfo addressInfo{};
        addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        addressInfo.buffer = handle;
        deviceAddress = vkGetBufferDeviceAddressKHR(device, &addressInfo);
    }
}

template<>
inline void Buffer<BufferType::HostVisible>::uploadDataDeffered(VkCommandBuffer cmd, VmaAllocator allocator, VkDevice device, const void* data, VkDeviceSize dataSize, VkDeviceSize offset)
{
    throw std::runtime_error("Do not use uploadDataDeffered on host visible buffer!");
}

template<>
inline void Buffer<BufferType::HostVisible>::uploadData(
    VmaAllocator allocator,
    VkDevice device,
    VkQueue queue,
    const void* data,
    VkDeviceSize dataSize,
    VkDeviceSize offset
)
{
    LOG_WARNING("USING UPLOAD DATA WITH HOST VISIBLE BUFFER IS DEPRECATED AND HIGHLY DISCOURAGED");

    VkCommandBuffer commandBuffer = VulkanContext::CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
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

    VulkanContext::SubmitCommandBuffer(commandBuffer, queue, true);
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
    VkBufferUsageFlags usage,
	PFN_vkGetBufferDeviceAddressKHR pfn_vkGetBufferDeviceAddressKHR,
    bool isLargeAllocation
)
{
	vkGetBufferDeviceAddressKHR = pfn_vkGetBufferDeviceAddressKHR;

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
	allocInfo.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    if (isLargeAllocation)
    {
        allocInfo.requiredFlags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    }
    
    VkResult result = vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &handle, &allocation, nullptr);
    if (result != VK_SUCCESS)
    {
        LOG_ERROR("Failed to create device-local buffer: " + vkResultToString(result));
    }

    if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
    {
        VkBufferDeviceAddressInfo addressInfo{};
        addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        addressInfo.buffer = handle;
        deviceAddress = vkGetBufferDeviceAddressKHR(device, &addressInfo);
    }
}

template<>
inline void Buffer<BufferType::DeviceLocal>::uploadData(
    VmaAllocator allocator,
    VkDevice device,
    VkQueue queue,
    const void* data,
    VkDeviceSize dataSize,
    VkDeviceSize offset
)
{
    VkCommandBuffer commandBuffer = VulkanContext::CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    if (data == nullptr)
    {
        LOG_ERROR("Cannot upload from null data pointer");
    }
    
    if (offset + dataSize > size)
    {
        LOG_ERROR("Upload size exceeds buffer size");
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
        LOG_ERROR("Failed to create staging buffer: " + vkResultToString(result));
    }
    
    stagingMappedMemory = stagingAllocationInfo.pMappedData;
    if (stagingMappedMemory == nullptr)
    {
        result = vmaMapMemory(allocator, stagingAllocation, &stagingMappedMemory);
        if (result != VK_SUCCESS || stagingMappedMemory == nullptr)
        {
            vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
            LOG_ERROR("Failed to map staging buffer memory: " + vkResultToString(result));
        }
    }
    
    BufferUtils::fastMemcpy(stagingMappedMemory, data, dataSize);
    
    if (stagingAllocationInfo.pMappedData == nullptr)
    {
        vmaUnmapMemory(allocator, stagingAllocation);
    }
    
    
    if (result != VK_SUCCESS)
    {
        vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
        LOG_ERROR("Failed to begin command buffer: " + vkResultToString(result));
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
    
    VulkanContext::SubmitCommandBuffer(commandBuffer, queue, true);
    
    vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
}

template<>
inline void Buffer<BufferType::DeviceLocal>::uploadDataDeffered(VkCommandBuffer cmd, VmaAllocator allocator, VkDevice device, const void* data, VkDeviceSize dataSize, VkDeviceSize offset)
{
	if (data == nullptr)
    {
        LOG_ERROR("Cannot upload from null data pointer");
    }
    
    if (offset + dataSize > size)
    {
        LOG_ERROR("Upload size exceeds buffer size");
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
        LOG_ERROR("Failed to create staging buffer: " + vkResultToString(result));
    }
    
    stagingMappedMemory = stagingAllocationInfo.pMappedData;
    if (stagingMappedMemory == nullptr)
    {
        result = vmaMapMemory(allocator, stagingAllocation, &stagingMappedMemory);
        if (result != VK_SUCCESS || stagingMappedMemory == nullptr)
        {
            vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
            LOG_ERROR("Failed to map staging buffer memory: " + vkResultToString(result));
        }
    }
    
    BufferUtils::fastMemcpy(stagingMappedMemory, data, dataSize);
    
    if (stagingAllocationInfo.pMappedData == nullptr)
    {
        vmaUnmapMemory(allocator, stagingAllocation);
    }
    
    
    if (result != VK_SUCCESS)
    {
        vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
        LOG_ERROR("Failed to begin command buffer: " + vkResultToString(result));
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
        cmd,
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
    vkCmdCopyBuffer(cmd, stagingBuffer, handle, 1, &copyRegion);
    
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
        cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0,
        0, nullptr,
        1, &postCopyBarrier,
        0, nullptr
    );

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
    LOG_ERROR("Direct updates not supported for device-local buffers. Use uploadData() instead.");
}

inline void cleanupBufferResources(VkDevice device)
{
    BufferUtils::FencePool::getInstance().cleanup(device);
} 

inline void ScratchBuffer::createScratchBuffer(VmaAllocator allocator, VkDevice device, VkDeviceSize size, PFN_vkGetBufferDeviceAddressKHR pfn_vkGetBufferDeviceAddressKHR)
{
    vkGetBufferDeviceAddressKHR = pfn_vkGetBufferDeviceAddressKHR;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &handle, &allocation, nullptr);

    VkBufferDeviceAddressInfoKHR bufferDeviceAddressInfo{};
    bufferDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    bufferDeviceAddressInfo.buffer = handle;
    deviceAddress = vkGetBufferDeviceAddressKHR(device, &bufferDeviceAddressInfo);
}

inline void ScratchBuffer::destroyScratchBuffer(VmaAllocator allocator)
{
    if (handle != VK_NULL_HANDLE)
    {
        vmaDestroyBuffer(allocator, handle, allocation);
        handle = VK_NULL_HANDLE;
        allocation = VK_NULL_HANDLE;
        deviceAddress = 0;
    }
}