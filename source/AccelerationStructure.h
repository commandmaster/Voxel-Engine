#pragma once

#include "vulkan/vulkan.h"

#include "Buffer.hpp"
#include "VulkanContext.hpp"

#include <vector>
#include <algorithm>

enum class BlasType
{
    Static,
    Dynamic,
};

struct GeometryRecord
{
    uint32_t aabbCount;
    uint32_t offset;
};

class GeometryPool
{
public:
    GeometryPool() = default;
    ~GeometryPool() = default;

    void init(uint32_t allocationSize)
    {
        this->allocationSize = allocationSize;
        aabbBuffers.reserve(allocationSize);
        records.reserve(allocationSize);
        aabbs.reserve(allocationSize); 
    }

    uint32_t addGeometry(VkAabbPositionsKHR* aabbData, uint32_t count)
    {
        // Validation
        if (count == 0 || aabbData == nullptr)
        {
            LOG_ERROR("Invalid geometry data");
            return UINT32_MAX;
        }

        uint32_t recordIndex = static_cast<uint32_t>(records.size());
        
        // Ensure we have enough capacity to avoid reallocation during insert
        if (aabbs.size() + count > aabbs.capacity())
        {
            aabbs.reserve(std::max(aabbs.capacity() * 2, aabbs.size() + count));
        }
        
        // Store record
        records.push_back({count, static_cast<uint32_t>(aabbs.size())});
        
        // Store AABBs
        aabbs.insert(aabbs.end(), aabbData, aabbData + count);
        
        // Create and upload buffer
        aabbBuffers.emplace_back();
        VkBufferUsageFlags bufferUsage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | 
                                         VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        
        aabbBuffers.back().create(
            VulkanContext::vmaAllocator, 
            VulkanContext::device, 
            count * sizeof(VkAabbPositionsKHR), 
            bufferUsage, 
            VulkanContext::vkGetBufferDeviceAddressKHR
        );
        aabbBuffers.back().updateData(VulkanContext::vmaAllocator, aabbData, count * sizeof(VkAabbPositionsKHR));
        
        return recordIndex;
    }

    void updateGeometry(uint32_t index, VkAabbPositionsKHR* aabbData, uint32_t count)
    {
        // Validation
        if (index >= records.size())
        {
            LOG_ERROR("Index out of bounds");
            return;
        }

        if (count == 0 || aabbData == nullptr)
        {
            LOG_ERROR("Invalid geometry data");
            return;
        }

        if (records[index].aabbCount != count)
        {
            LOG_ERROR("Cannot update geometry with a different number of primitives");
            return;
        }

        // Update buffer directly
        aabbBuffers[index].updateData(VulkanContext::vmaAllocator, aabbData, count * sizeof(VkAabbPositionsKHR));
        
        // Update the local aabb copy as well
        const uint32_t offset = records[index].offset;
        if (offset + count <= aabbs.size())
        {
            // Direct memcpy for better performance
            std::memcpy(aabbs.data() + offset, aabbData, count * sizeof(VkAabbPositionsKHR));
        }
    }

    void destroy()
    {
        for (auto& buffer : aabbBuffers)
        {
            buffer.destroy(VulkanContext::vmaAllocator);
        }
        records.clear();
        aabbs.clear();
        aabbBuffers.clear();
    }

    const GeometryRecord& getRecord(uint32_t index) const
    {
        return records[index];
    }

    const Buffer<BufferType::DeviceLocal>& getBuffer(uint32_t index) const
    {
        return aabbBuffers[index];
    }

    const std::vector<VkAabbPositionsKHR>& getAabbs() const
    {
        return aabbs;
    }

    bool isValid(uint32_t index) const
    {
        return index < records.size();
    }

private:
    std::vector<VkAabbPositionsKHR> aabbs;
    std::vector<GeometryRecord> records;
    std::vector<Buffer<BufferType::DeviceLocal>> aabbBuffers;
    uint32_t allocationSize;

    friend class BLAS<BlasType::Static>;
    friend class BLAS<BlasType::Dynamic>;
};

template <BlasType Type>
class BLAS
{
public:
    BLAS() = default;
    ~BLAS();

    void init(GeometryPool* pool, const std::vector<VkAabbPositionsKHR>& aabbs);
    void cleanup();
    void rebuild(const std::vector<VkAabbPositionsKHR>& aabbs);
    
    void update(const std::vector<VkAabbPositionsKHR>& aabbs) { static_assert(Type == BlasType::Dynamic, "update() is only available for Dynamic BLAS"); }
    
    // Accessors
    uint64_t getDeviceAddress() const { return deviceAddress; }
    VkAccelerationStructureKHR getHandle() const { return handle; }
    GeometryPool* getGeometryPool() const { return geometryPool; }
    uint32_t getGeometryIndex() const { return geometryIndex; }
    
private:
    VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
    uint64_t deviceAddress = 0;
    Buffer<BufferType::DeviceLocal> accelerationStructureStorage;
    ScratchBuffer scratchBuffer;
    GeometryPool* geometryPool = nullptr;
    uint32_t geometryIndex = UINT32_MAX;
};

class AccelerationStructureManager
{
public:
    AccelerationStructureManager() = default;
    ~AccelerationStructureManager() = default;
    
    void init();
    void rebuildTopLevelAS(bool update = false);
    void cleanup();

    void addStaticBLAS(const std::vector<VkAabbPositionsKHR>& aabbs);
    void addDynamicBLAS(const std::vector<VkAabbPositionsKHR>& aabbs);

    // Accessors
    VkAccelerationStructureKHR getTLAS() const { return topLevelAccelerationStructure; }
    uint64_t getTLASDeviceAddress() const { return topLevelAccelerationStructureDeviceAddress; }

private:
    std::vector<BLAS<BlasType::Static>> staticBlas;
    std::vector<BLAS<BlasType::Dynamic>> dynamicBlas;

    GeometryPool geometryPool;

    VkAccelerationStructureKHR topLevelAccelerationStructure = VK_NULL_HANDLE;
    uint64_t topLevelAccelerationStructureDeviceAddress = 0;
    Buffer<BufferType::DeviceLocal> topLevelAccelerationStructureStorage;
    ScratchBuffer topLevelScratchBuffer;
    uint32_t instanceCount = 0; // Track instance count for rebuild optimization
};

template <BlasType Type>
inline void BLAS<Type>::cleanup()
{
    if (handle != VK_NULL_HANDLE)
    {
        // Make sure any pending operations are complete
        vkDeviceWaitIdle(VulkanContext::device);
        
        VulkanContext::vkDestroyAccelerationStructureKHR(VulkanContext::device, handle, nullptr);
        handle = VK_NULL_HANDLE;
    }
    
    scratchBuffer.destroyScratchBuffer(VulkanContext::vmaAllocator);
    accelerationStructureStorage.destroy(VulkanContext::vmaAllocator);
    deviceAddress = 0;
    geometryIndex = UINT32_MAX;
    geometryPool = nullptr;
}

template <BlasType Type>
inline BLAS<Type>::~BLAS()
{
    cleanup();
}

template<>
inline void BLAS<BlasType::Static>::rebuild(const std::vector<VkAabbPositionsKHR>& aabbs)
{
    if (handle == VK_NULL_HANDLE || geometryPool == nullptr || geometryIndex == UINT32_MAX)
    {
        LOG_ERROR("Cannot rebuild invalid BLAS");
        return;
    }

    if (aabbs.empty())
    {
        LOG_ERROR("Cannot rebuild with empty AABBs");
        return;
    }

    // Update geometry data in the pool
    geometryPool->updateGeometry(geometryIndex, const_cast<VkAabbPositionsKHR*>(aabbs.data()), static_cast<uint32_t>(aabbs.size()));
    
    const GeometryRecord& record = geometryPool->getRecord(geometryIndex);
    const Buffer<BufferType::DeviceLocal>& aabbBuffer = geometryPool->getBuffer(geometryIndex);
    
    VkDeviceOrHostAddressConstKHR aabbDataDeviceAddress{};
    aabbDataDeviceAddress.deviceAddress = aabbBuffer.deviceAddress;
    
    VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
    accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_AABBS_KHR;
    accelerationStructureGeometry.flags = 0;
    
    accelerationStructureGeometry.geometry.aabbs.sType = 
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR;
    accelerationStructureGeometry.geometry.aabbs.data = aabbDataDeviceAddress;
    accelerationStructureGeometry.geometry.aabbs.stride = sizeof(VkAabbPositionsKHR);

    const uint32_t primitiveCount = static_cast<uint32_t>(aabbs.size());
    
    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.dstAccelerationStructure = handle;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &accelerationStructureGeometry;
    buildInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

    VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
    buildRangeInfo.primitiveCount = primitiveCount;
    buildRangeInfo.primitiveOffset = 0;
    buildRangeInfo.firstVertex = 0;
    buildRangeInfo.transformOffset = 0;

    std::vector<VkAccelerationStructureBuildRangeInfoKHR*> buildRangeInfos = {&buildRangeInfo};

    VkCommandBuffer cmd = VulkanContext::CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    
    // Add memory barrier to ensure the data is visible to the build operation
    VkMemoryBarrier memoryBarrier{};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT; 
    memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    
    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        0,
        1, &memoryBarrier,
        0, nullptr,
        0, nullptr
    );
    
    VulkanContext::vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, buildRangeInfos.data());
    
    // Add barrier to ensure build is complete before any subsequent operation
    memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
    
    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        0,
        1, &memoryBarrier,
        0, nullptr,
        0, nullptr
    );
    
    VulkanContext::SubmitCommandBuffer(cmd, VulkanContext::graphicsQueue, true);
}

template<>
inline void BLAS<BlasType::Static>::init(GeometryPool* pool, const std::vector<VkAabbPositionsKHR>& aabbs)
{
    if (aabbs.empty() || pool == nullptr)
    {
        return;
    }

    geometryPool = pool;
    geometryIndex = pool->addGeometry(const_cast<VkAabbPositionsKHR*>(aabbs.data()), static_cast<uint32_t>(aabbs.size()));

    VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
    accelerationStructureBuildGeometryInfo.sType = 
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    accelerationStructureBuildGeometryInfo.type = 
        VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    accelerationStructureBuildGeometryInfo.flags = 
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    accelerationStructureBuildGeometryInfo.geometryCount = 1;

    VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
    accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_AABBS_KHR;
    accelerationStructureGeometry.flags = 0;
    accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

    const uint32_t primitiveCount = static_cast<uint32_t>(aabbs.size());

    VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
    accelerationStructureBuildSizesInfo.sType = 
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    VulkanContext::vkGetAccelerationStructureBuildSizesKHR(
        VulkanContext::device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &accelerationStructureBuildGeometryInfo,
        &primitiveCount,
        &accelerationStructureBuildSizesInfo);

    accelerationStructureStorage.create(
        VulkanContext::vmaAllocator,
        VulkanContext::device,
        accelerationStructureBuildSizesInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | 
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VulkanContext::vkGetBufferDeviceAddressKHR
    );

    VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
    accelerationStructureCreateInfo.sType = 
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    accelerationStructureCreateInfo.buffer = accelerationStructureStorage.handle;
    accelerationStructureCreateInfo.size = 
        accelerationStructureBuildSizesInfo.accelerationStructureSize;
    accelerationStructureCreateInfo.type = 
        VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    
    VkResult result = VulkanContext::vkCreateAccelerationStructureKHR(
        VulkanContext::device,
        &accelerationStructureCreateInfo,
        nullptr,
        &handle);
        
    if (result != VK_SUCCESS)
    {
        // Failed to create acceleration structure
        cleanup();
        return;
    }

    scratchBuffer.createScratchBuffer(
        VulkanContext::vmaAllocator, 
        VulkanContext::device, 
        accelerationStructureBuildSizesInfo.buildScratchSize, 
        VulkanContext::vkGetBufferDeviceAddressKHR
    );

    rebuild(aabbs);

    VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
    addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    addressInfo.accelerationStructure = handle;
    deviceAddress = VulkanContext::vkGetAccelerationStructureDeviceAddressKHR(VulkanContext::device, &addressInfo);
}

template<>
inline void BLAS<BlasType::Dynamic>::rebuild(const std::vector<VkAabbPositionsKHR>& aabbs)
{
    if (handle == VK_NULL_HANDLE || geometryPool == nullptr || geometryIndex == UINT32_MAX)
    {
        LOG_ERROR("Cannot rebuild without a valid handle");
        return;
    }

    if (aabbs.empty())
    {
        LOG_ERROR("Cannot rebuild with empty AABBs");
        return;
    }

    // Update geometry data in the pool
    geometryPool->updateGeometry(geometryIndex, const_cast<VkAabbPositionsKHR*>(aabbs.data()), static_cast<uint32_t>(aabbs.size()));
    
    const GeometryRecord& record = geometryPool->getRecord(geometryIndex);
    const Buffer<BufferType::DeviceLocal>& aabbBuffer = geometryPool->getBuffer(geometryIndex);
    
    VkDeviceOrHostAddressConstKHR aabbDataDeviceAddress{};
    aabbDataDeviceAddress.deviceAddress = aabbBuffer.deviceAddress;
    
    VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
    accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_AABBS_KHR;
    accelerationStructureGeometry.flags = VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;
    
    accelerationStructureGeometry.geometry.aabbs.sType = 
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR;
    accelerationStructureGeometry.geometry.aabbs.data = aabbDataDeviceAddress;
    accelerationStructureGeometry.geometry.aabbs.stride = sizeof(VkAabbPositionsKHR);

    const uint32_t primitiveCount = static_cast<uint32_t>(aabbs.size());
    
    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
                      VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;
    buildInfo.dstAccelerationStructure = handle;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &accelerationStructureGeometry;
    buildInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

    VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
    buildRangeInfo.primitiveCount = primitiveCount;
    buildRangeInfo.primitiveOffset = 0;
    buildRangeInfo.firstVertex = 0;
    buildRangeInfo.transformOffset = 0;

    std::vector<VkAccelerationStructureBuildRangeInfoKHR*> buildRangeInfos = {&buildRangeInfo};

    VkCommandBuffer cmd = VulkanContext::CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    
    // Add memory barrier to ensure the data is visible to the build operation
    VkMemoryBarrier memoryBarrier{};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT; 
    memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    
    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        0,
        1, &memoryBarrier,
        0, nullptr,
        0, nullptr
    );
    
    VulkanContext::vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, buildRangeInfos.data());
    
    // Add barrier to ensure build is complete before any subsequent operation
    memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
    
    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        0,
        1, &memoryBarrier,
        0, nullptr,
        0, nullptr
    );
    
    VulkanContext::SubmitCommandBuffer(cmd, VulkanContext::graphicsQueue, true);
}

template<>
inline void BLAS<BlasType::Dynamic>::init(GeometryPool* pool, const std::vector<VkAabbPositionsKHR>& aabbs)
{
    if (aabbs.empty() || pool == nullptr)
    {
        LOG_ERROR("Cannot create an acceleration structure with no AABBs or null geometry pool");
        return;
    }

    geometryPool = pool;
    geometryIndex = pool->addGeometry(const_cast<VkAabbPositionsKHR*>(aabbs.data()), static_cast<uint32_t>(aabbs.size()));

    VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
    accelerationStructureBuildGeometryInfo.sType = 
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    accelerationStructureBuildGeometryInfo.type = 
        VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    accelerationStructureBuildGeometryInfo.flags = 
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
        VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
    accelerationStructureBuildGeometryInfo.geometryCount = 1;

    VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
    accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_AABBS_KHR;
    accelerationStructureGeometry.flags = VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;
    accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

    const uint32_t primitiveCount = static_cast<uint32_t>(aabbs.size());

    VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
    accelerationStructureBuildSizesInfo.sType = 
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    VulkanContext::vkGetAccelerationStructureBuildSizesKHR(
        VulkanContext::device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &accelerationStructureBuildGeometryInfo,
        &primitiveCount,
        &accelerationStructureBuildSizesInfo);

    accelerationStructureStorage.create(
        VulkanContext::vmaAllocator,
        VulkanContext::device,
        accelerationStructureBuildSizesInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | 
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VulkanContext::vkGetBufferDeviceAddressKHR
    );

    VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
    accelerationStructureCreateInfo.sType = 
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    accelerationStructureCreateInfo.buffer = accelerationStructureStorage.handle;
    accelerationStructureCreateInfo.size = 
        accelerationStructureBuildSizesInfo.accelerationStructureSize;
    accelerationStructureCreateInfo.type = 
        VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    
    VkResult result = VulkanContext::vkCreateAccelerationStructureKHR(
        VulkanContext::device,
        &accelerationStructureCreateInfo,
        nullptr,
        &handle);
        
    if (result != VK_SUCCESS)
    {
        LOG_ERROR("Failed to create acceleration structure");
        cleanup();
        return;
    }

    scratchBuffer.createScratchBuffer(
        VulkanContext::vmaAllocator, 
        VulkanContext::device, 
        accelerationStructureBuildSizesInfo.buildScratchSize, 
        VulkanContext::vkGetBufferDeviceAddressKHR
    );

    rebuild(aabbs);

    VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
    addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    addressInfo.accelerationStructure = handle;
    deviceAddress = VulkanContext::vkGetAccelerationStructureDeviceAddressKHR(VulkanContext::device, &addressInfo);
}

template<>
inline void BLAS<BlasType::Dynamic>::update(const std::vector<VkAabbPositionsKHR>& aabbs)
{
    if (handle == VK_NULL_HANDLE || geometryPool == nullptr || geometryIndex == UINT32_MAX)
    {
        LOG_ERROR("Cannot update without a valid handle");
        return;
    }
    
    if (aabbs.empty())
    {
        LOG_ERROR("Cannot update with empty AABBs");
        return;
    }

    const GeometryRecord& record = geometryPool->getRecord(geometryIndex);
    
    // Ensure we're updating with the same number of primitives
    if (record.aabbCount != aabbs.size())
    {
        LOG_VERBOSE("Cannot update with a different number of primitives (expected: %u, actual: %zu)", 
                   record.aabbCount, aabbs.size());
        rebuild(aabbs);
        return;
    }
    
    // Update geometry data in the pool
    geometryPool->updateGeometry(geometryIndex, const_cast<VkAabbPositionsKHR*>(aabbs.data()), static_cast<uint32_t>(aabbs.size()));
    const Buffer<BufferType::DeviceLocal>& aabbBuffer = geometryPool->getBuffer(geometryIndex);
    
    VkDeviceOrHostAddressConstKHR aabbDataDeviceAddress{};
    aabbDataDeviceAddress.deviceAddress = aabbBuffer.deviceAddress;

    VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
    accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_AABBS_KHR;
    accelerationStructureGeometry.flags = VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;
    
    accelerationStructureGeometry.geometry.aabbs.sType = 
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR;
    accelerationStructureGeometry.geometry.aabbs.data = aabbDataDeviceAddress;
    accelerationStructureGeometry.geometry.aabbs.stride = sizeof(VkAabbPositionsKHR);

    const uint32_t primitiveCount = static_cast<uint32_t>(aabbs.size());
    
    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
                      VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;
    buildInfo.srcAccelerationStructure = handle;
    buildInfo.dstAccelerationStructure = handle;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &accelerationStructureGeometry;
    buildInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

    VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
    buildRangeInfo.primitiveCount = primitiveCount;
    buildRangeInfo.primitiveOffset = 0;
    buildRangeInfo.firstVertex = 0;
    buildRangeInfo.transformOffset = 0;

    std::vector<VkAccelerationStructureBuildRangeInfoKHR*> buildRangeInfos = {&buildRangeInfo};

    VkCommandBuffer cmd = VulkanContext::CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    
    // Add memory barrier to ensure the data is visible to the build operation
    VkMemoryBarrier memoryBarrier{};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT; 
    memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    
    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        0,
        1, &memoryBarrier,
        0, nullptr,
        0, nullptr
    );
    
    VulkanContext::vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, buildRangeInfos.data());
    
    // Add barrier to ensure build is complete before any subsequent operation
    memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
    
    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        0,
        1, &memoryBarrier,
        0, nullptr,
        0, nullptr
    );
    
    VulkanContext::SubmitCommandBuffer(cmd, VulkanContext::graphicsQueue, true);
}

void AccelerationStructureManager::init()
{
    geometryPool.init(1024); // Reasonable initial size for the geometry pool
}

void AccelerationStructureManager::rebuildTopLevelAS(bool update)
{
    if (staticBlas.empty() && dynamicBlas.empty())
    {
        LOG_ERROR("Cannot rebuild TLAS without any BLAS instances");
        return;
    }

    const uint32_t blasCount = static_cast<uint32_t>(staticBlas.size() + dynamicBlas.size());
    
    // Pre-allocate the instances vector to avoid reallocations
    std::vector<VkAccelerationStructureInstanceKHR> instances;
    instances.reserve(blasCount);
    
    // Add instances for static BLAS
    for (size_t i = 0; i < staticBlas.size(); i++) 
    {
        if (staticBlas[i].getHandle() == VK_NULL_HANDLE)
        {
            LOG_ERROR("Skipping invalid static BLAS at index %zu", i);
            continue;
        }

        VkAccelerationStructureInstanceKHR instance{};
        instance.instanceCustomIndex = static_cast<uint32_t>(i);
        instance.mask = 0xFF;
        instance.instanceShaderBindingTableRecordOffset = 0;
        instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        instance.accelerationStructureReference = staticBlas[i].getDeviceAddress();
        
        // Identity transform matrix
        instance.transform = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f
        };
        
        instances.push_back(instance);
    }
    
    // Add instances for dynamic BLAS
    for (size_t i = 0; i < dynamicBlas.size(); i++) 
    {
        if (dynamicBlas[i].getHandle() == VK_NULL_HANDLE)
        {
            LOG_ERROR("Skipping invalid dynamic BLAS at index %zu", i);
            continue;
        }

        VkAccelerationStructureInstanceKHR instance{};
        instance.instanceCustomIndex = static_cast<uint32_t>(i + staticBlas.size());
        instance.mask = 0xFF;
        instance.instanceShaderBindingTableRecordOffset = 0;
        instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        instance.accelerationStructureReference = dynamicBlas[i].getDeviceAddress();
        
        // Identity transform matrix
        instance.transform = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f
        };
        
        instances.push_back(instance);
    }

    if (instances.empty())
    {
        LOG_ERROR("Cannot rebuild TLAS: No valid BLAS instances found");
        return;
    }
    
    // Create and upload the instance buffer
    Buffer<BufferType::DeviceLocal> instanceBuffer;
    instanceBuffer.create(
        VulkanContext::vmaAllocator,
        VulkanContext::device,
        instances.size() * sizeof(VkAccelerationStructureInstanceKHR),
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VulkanContext::vkGetBufferDeviceAddressKHR
    );
    instanceBuffer.updateData(VulkanContext::vmaAllocator, instances.data(), instances.size() * sizeof(VkAccelerationStructureInstanceKHR));
    
    VkDeviceOrHostAddressConstKHR instanceDataDeviceAddress{};
    instanceDataDeviceAddress.deviceAddress = instanceBuffer.deviceAddress;
    
    VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
    accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    accelerationStructureGeometry.flags = 0;
    accelerationStructureGeometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    accelerationStructureGeometry.geometry.instances.arrayOfPointers = VK_FALSE;
    accelerationStructureGeometry.geometry.instances.data = instanceDataDeviceAddress;
    
    const uint32_t primitiveCount = static_cast<uint32_t>(instances.size());
    
    // Create or update TLAS
    if (topLevelAccelerationStructure == VK_NULL_HANDLE || !update || 
        (update && instanceCount != primitiveCount)) // Recreate if instance count changed
    {
        // If we're rebuilding from scratch or it doesn't exist
        if (topLevelAccelerationStructure != VK_NULL_HANDLE)
        {
            VulkanContext::vkDestroyAccelerationStructureKHR(VulkanContext::device, topLevelAccelerationStructure, nullptr);
            topLevelAccelerationStructureStorage.destroy(VulkanContext::vmaAllocator);
            topLevelScratchBuffer.destroyScratchBuffer(VulkanContext::vmaAllocator);
            topLevelAccelerationStructure = VK_NULL_HANDLE;
        }
        
        VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
        accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
                                                       VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
        accelerationStructureBuildGeometryInfo.geometryCount = 1;
        accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;
        
        VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
        accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        VulkanContext::vkGetAccelerationStructureBuildSizesKHR(
            VulkanContext::device,
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &accelerationStructureBuildGeometryInfo,
            &primitiveCount,
            &accelerationStructureBuildSizesInfo);
            
        topLevelAccelerationStructureStorage.create(
            VulkanContext::vmaAllocator,
            VulkanContext::device,
            accelerationStructureBuildSizesInfo.accelerationStructureSize,
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VulkanContext::vkGetBufferDeviceAddressKHR
        );
        
        VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
        accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        accelerationStructureCreateInfo.buffer = topLevelAccelerationStructureStorage.handle;
        accelerationStructureCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
        accelerationStructureCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        
        VkResult result = VulkanContext::vkCreateAccelerationStructureKHR(
            VulkanContext::device,
            &accelerationStructureCreateInfo,
            nullptr,
            &topLevelAccelerationStructure);
            
        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to create top level acceleration structure");
            topLevelAccelerationStructureStorage.destroy(VulkanContext::vmaAllocator);
            instanceBuffer.destroy(VulkanContext::vmaAllocator);
            return;
        }
        
        topLevelScratchBuffer.createScratchBuffer(
            VulkanContext::vmaAllocator,
            VulkanContext::device,
            accelerationStructureBuildSizesInfo.buildScratchSize,
            VulkanContext::vkGetBufferDeviceAddressKHR
        );
        
        VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
        buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
                          VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
        buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        buildInfo.dstAccelerationStructure = topLevelAccelerationStructure;
        buildInfo.geometryCount = 1;
        buildInfo.pGeometries = &accelerationStructureGeometry;
        buildInfo.scratchData.deviceAddress = topLevelScratchBuffer.deviceAddress;
        
        VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
        buildRangeInfo.primitiveCount = primitiveCount;
        buildRangeInfo.primitiveOffset = 0;
        buildRangeInfo.firstVertex = 0;
        buildRangeInfo.transformOffset = 0;
        
        std::vector<VkAccelerationStructureBuildRangeInfoKHR*> buildRangeInfos = { &buildRangeInfo };
        
        VkCommandBuffer cmd = VulkanContext::CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
        
        // Add memory barrier to ensure instance data is visible
        VkMemoryBarrier memoryBarrier{};
        memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        memoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        
        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            0,
            1, &memoryBarrier,
            0, nullptr,
            0, nullptr
        );
        
        VulkanContext::vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, buildRangeInfos.data());
        
        // Add barrier to ensure build is complete before any subsequent operation
        memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        
        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            0,
            1, &memoryBarrier,
            0, nullptr,
            0, nullptr
        );
        
        VulkanContext::SubmitCommandBuffer(cmd, VulkanContext::graphicsQueue, true);
        instanceCount = primitiveCount;
    }
    else if (update)
    {
        // Update the existing acceleration structure
        VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
        buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | 
                         VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
        buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;
        buildInfo.srcAccelerationStructure = topLevelAccelerationStructure;
        buildInfo.dstAccelerationStructure = topLevelAccelerationStructure;
        buildInfo.geometryCount = 1;
        buildInfo.pGeometries = &accelerationStructureGeometry;
        buildInfo.scratchData.deviceAddress = topLevelScratchBuffer.deviceAddress;
        
        VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
        buildRangeInfo.primitiveCount = primitiveCount;
        buildRangeInfo.primitiveOffset = 0;
        buildRangeInfo.firstVertex = 0;
        buildRangeInfo.transformOffset = 0;
        
        std::vector<VkAccelerationStructureBuildRangeInfoKHR*> buildRangeInfos = { &buildRangeInfo };
        
        VkCommandBuffer cmd = VulkanContext::CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
        
        // Add memory barrier to ensure instance data is visible
        VkMemoryBarrier memoryBarrier{};
        memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        memoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        
        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            0,
            1, &memoryBarrier,
            0, nullptr,
            0, nullptr
        );
        
        VulkanContext::vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, buildRangeInfos.data());
        
        // Add barrier to ensure build is complete before any subsequent operation
        memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        
        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            0,
            1, &memoryBarrier,
            0, nullptr,
            0, nullptr
        );
        
        VulkanContext::SubmitCommandBuffer(cmd, VulkanContext::graphicsQueue, true);
    }
    
    // Get the device address for the TLAS
    if (topLevelAccelerationStructure != VK_NULL_HANDLE)
    {
        VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
        addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        addressInfo.accelerationStructure = topLevelAccelerationStructure;
        topLevelAccelerationStructureDeviceAddress = VulkanContext::vkGetAccelerationStructureDeviceAddressKHR(VulkanContext::device, &addressInfo);
    }
    
    instanceBuffer.destroy(VulkanContext::vmaAllocator);
}

void AccelerationStructureManager::cleanup()
{
    if (topLevelAccelerationStructure != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(VulkanContext::device);
        VulkanContext::vkDestroyAccelerationStructureKHR(VulkanContext::device, topLevelAccelerationStructure, nullptr);
        topLevelAccelerationStructure = VK_NULL_HANDLE;
    }
    
    topLevelAccelerationStructureStorage.destroy(VulkanContext::vmaAllocator);
    topLevelScratchBuffer.destroyScratchBuffer(VulkanContext::vmaAllocator);
    
    staticBlas.clear();
    dynamicBlas.clear();
    
    geometryPool.destroy();
}

void AccelerationStructureManager::addStaticBLAS(const std::vector<VkAabbPositionsKHR>& aabbs)
{
    if (aabbs.empty())
    {
        return;
    }
    
    staticBlas.push_back(BLAS<BlasType::Static>());
    staticBlas.back().init(&geometryPool, aabbs);
}

void AccelerationStructureManager::addDynamicBLAS(const std::vector<VkAabbPositionsKHR>& aabbs)
{
    if (aabbs.empty())
    {
        return;
    }
    
    dynamicBlas.push_back(BLAS<BlasType::Dynamic>());
    dynamicBlas.back().init(&geometryPool, aabbs);
}




