#include "MemoryClasses.hpp"
#include "VoxelEngine.hpp"

void ScratchBuffer::createScratchBuffer(VmaAllocator allocator, VkDevice device, VkDeviceSize size, ScratchBuffer& scratchBuffer)
{
    // Create scratch buffer
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &scratchBuffer.handle, &scratchBuffer.allocation, nullptr);

    // Get the device address for the scratch buffer
    VkBufferDeviceAddressInfoKHR bufferDeviceAddressInfo{};
    bufferDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    bufferDeviceAddressInfo.buffer = scratchBuffer.handle;
    scratchBuffer.deviceAddress = VoxelEngine::vkGetBufferDeviceAddressKHR(device, &bufferDeviceAddressInfo);
}

void ScratchBuffer::destroyScratchBuffer(VmaAllocator allocator, ScratchBuffer& scratchBuffer)
{
    if (scratchBuffer.handle != VK_NULL_HANDLE)
    {
        vmaDestroyBuffer(allocator, scratchBuffer.handle, scratchBuffer.allocation);
        scratchBuffer.handle = VK_NULL_HANDLE;
        scratchBuffer.allocation = VK_NULL_HANDLE;
        scratchBuffer.deviceAddress = 0;
    }
}

void ManagedBuffer::create(
    VmaAllocator allocator,
    VkDevice device,
    VkDeviceSize bufferSize,
    VkBufferUsageFlags usage,
    BufferType bufferType
)
{
    size = bufferSize;
    type = bufferType;

    // Always add this flag for all buffers (needed for ray tracing)
    usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;

    if (type == BufferType::HostVisible)
    {
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    }
    else
    {
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    }

    if (vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &handle, &allocation, nullptr) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create buffer!");
    }

    // Get the device address if needed for ray tracing
    VkBufferDeviceAddressInfoKHR bufferDeviceAddressInfo{};
    bufferDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    bufferDeviceAddressInfo.buffer = handle;
    deviceAddress = VoxelEngine::vkGetBufferDeviceAddressKHR(device, &bufferDeviceAddressInfo);

    _isDestroyed = false;
}

void ManagedBuffer::uploadData(
    VmaAllocator allocator,
    VkDevice device,
    VkCommandBuffer commandBuffer,
    VkQueue queue,
    const void* data,
    VkDeviceSize dataSize,
    VkDeviceSize offset
)
{
    // Create a staging buffer
    ManagedBuffer stagingBuffer;
    stagingBuffer.create(
        allocator,
        device,
        dataSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        BufferType::HostVisible
    );

    // Copy data to staging buffer
    stagingBuffer.updateData(allocator, data, dataSize);

    // Record command buffer for transfer
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = offset;
    copyRegion.size = dataSize;
    vkCmdCopyBuffer(commandBuffer, stagingBuffer.handle, handle, 1, &copyRegion);

    vkEndCommandBuffer(commandBuffer);

    // Submit and wait for completion
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

    vkDestroyFence(device, fence, nullptr);
    
    // Clean up the staging buffer
    stagingBuffer.destroy(allocator);
} 