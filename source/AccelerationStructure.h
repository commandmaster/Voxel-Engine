#pragma once

#include "vulkan/vulkan.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

#include "Buffer.hpp"
#include "VulkanContext.hpp"
#include "PerformanceTimer.hpp"

#include <vector>
#include <algorithm>
#include <stdexcept>
#include <memory>
#include <iostream>


class BLAS
{
public:
	BLAS() = default;
	~BLAS() = default;

    BLAS(const BLAS&) = delete;
    BLAS& operator=(const BLAS&) = delete;
    BLAS(BLAS&& other) noexcept { *this = std::move(other); }
    BLAS& operator=(BLAS&& other) noexcept {
        if (this != &other) {
            destroy(); // Clean up existing resources
            m_blasHandle = other.m_blasHandle;
            m_deviceAddress = other.m_deviceAddress;
            m_primitiveCount = other.m_primitiveCount;
            m_buffer = std::move(other.m_buffer);
            m_aabbBuffer = std::move(other.m_aabbBuffer);
            m_scratchBuffer = std::move(other.m_scratchBuffer);
            m_geometry = other.m_geometry;
            m_buildInfo = other.m_buildInfo;
            m_buildScratchSize = other.m_buildScratchSize;
            m_updateScratchSize = other.m_updateScratchSize;

            // Invalidate the source object
            other.m_blasHandle = VK_NULL_HANDLE;
            other.m_deviceAddress = 0;
            other.m_primitiveCount = 0;
        }
        return *this;
    }

	void init(const VkAabbPositionsKHR* initialAABBData, uint32_t aabbDataSize)
	{
        if (initialAABBData == nullptr || aabbDataSize == 0) {
            throw std::runtime_error("Initial AABB data cannot be null or empty for BLAS init.");
        }
        if (aabbDataSize % sizeof(VkAabbPositionsKHR) != 0) {
             throw std::runtime_error("AABB data size is not a multiple of VkAabbPositionsKHR size.");
        }
		m_primitiveCount = aabbDataSize / sizeof(VkAabbPositionsKHR);

		const VkBufferUsageFlags aabbBufferUsageFlags =
			VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

		m_aabbBuffer.create(
			VulkanContext::vmaAllocator,
			VulkanContext::device,
			aabbDataSize,
			aabbBufferUsageFlags,
			VulkanContext::vkGetBufferDeviceAddressKHR
		);

		VkDeviceOrHostAddressConstKHR aabbDataDeviceAddress{};
		aabbDataDeviceAddress.deviceAddress = m_aabbBuffer.deviceAddress;

		m_geometry = {};
		m_geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
		m_geometry.geometryType = VK_GEOMETRY_TYPE_AABBS_KHR;
		m_geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
		m_geometry.geometry.aabbs.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR;
		m_geometry.geometry.aabbs.data = aabbDataDeviceAddress;
		m_geometry.geometry.aabbs.stride = sizeof(VkAabbPositionsKHR);

		m_buildInfo = {};
		m_buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		m_buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		m_buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
		m_buildInfo.geometryCount = 1;
		m_buildInfo.pGeometries = &m_geometry;

		VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo{};
		buildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
		VulkanContext::vkGetAccelerationStructureBuildSizesKHR(
			VulkanContext::device,
			VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
			&m_buildInfo,
			&m_primitiveCount,
			&buildSizesInfo);

        if (buildSizesInfo.accelerationStructureSize == 0) 
        {
            throw std::runtime_error("Acceleration structure size query returned 0.");
        }
        m_buildScratchSize = buildSizesInfo.buildScratchSize;
        m_updateScratchSize = buildSizesInfo.updateScratchSize;


		m_buffer.create(
			VulkanContext::vmaAllocator,
			VulkanContext::device,
			buildSizesInfo.accelerationStructureSize,
			VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			VulkanContext::vkGetBufferDeviceAddressKHR
		);

		VkAccelerationStructureCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
		createInfo.buffer = m_buffer.handle;
		createInfo.size = buildSizesInfo.accelerationStructureSize;
		createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

		VkResult result = VulkanContext::vkCreateAccelerationStructureKHR(
			VulkanContext::device,
			&createInfo,
			nullptr,
			&m_blasHandle);
        if (result != VK_SUCCESS) {
             throw std::runtime_error("Failed to create acceleration structure handle.");
        }

        VkDeviceSize scratchSize = std::max(m_buildScratchSize, m_updateScratchSize);
		m_scratchBuffer.createScratchBuffer(
            VulkanContext::vmaAllocator,
            VulkanContext::device,
            scratchSize,
            VulkanContext::vkGetBufferDeviceAddressKHR
        );

		VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
		addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
		addressInfo.accelerationStructure = m_blasHandle;
		m_deviceAddress = VulkanContext::vkGetAccelerationStructureDeviceAddressKHR(VulkanContext::device, &addressInfo);

        build(initialAABBData, aabbDataSize);
	}

	void build(const VkAabbPositionsKHR* aabbData, uint32_t aabbDataSize)
	{
        if (m_blasHandle == VK_NULL_HANDLE) {
            throw std::runtime_error("BLAS must be initialized before building.");
        }
        if (aabbData == nullptr || aabbDataSize == 0) {
            throw std::runtime_error("AABB data cannot be null or empty for BLAS build.");
        }
        if (aabbDataSize != m_aabbBuffer.size) {
            throw std::runtime_error("AABB data size in build() does not match size during init().");
        }

		m_aabbBuffer.updateData(VulkanContext::vmaAllocator, aabbData, aabbDataSize);

		VkAccelerationStructureBuildGeometryInfoKHR buildInfo = m_buildInfo;
		buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		buildInfo.dstAccelerationStructure = m_blasHandle;
        buildInfo.scratchData.deviceAddress = m_scratchBuffer.deviceAddress;
        buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;
        buildInfo.pGeometries = &m_geometry;


		VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
		buildRangeInfo.primitiveCount = m_primitiveCount;
		buildRangeInfo.primitiveOffset = 0;
		buildRangeInfo.firstVertex = 0;
		buildRangeInfo.transformOffset = 0;

		const VkAccelerationStructureBuildRangeInfoKHR* pBuildRangeInfo = &buildRangeInfo;

		VkCommandBuffer cmd = VulkanContext::CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

        VkMemoryBarrier memoryBarrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
        memoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_HOST_BIT,
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            0,
            1, &memoryBarrier,
            0, nullptr,
            0, nullptr
        );

		VulkanContext::vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pBuildRangeInfo);

        memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
            0,
            1, &memoryBarrier,
            0, nullptr,
            0, nullptr
        );


		VulkanContext::SubmitCommandBuffer(cmd, VulkanContext::graphicsQueue, true);
	}

	void destroy()
	{
		if (m_blasHandle != VK_NULL_HANDLE) {
			VulkanContext::vkDestroyAccelerationStructureKHR(VulkanContext::device, m_blasHandle, nullptr);
			m_blasHandle = VK_NULL_HANDLE;
		}
		m_buffer.destroy(VulkanContext::vmaAllocator);
		m_scratchBuffer.destroyScratchBuffer(VulkanContext::vmaAllocator);
		m_aabbBuffer.destroy(VulkanContext::vmaAllocator);

        m_deviceAddress = 0;
        m_primitiveCount = 0;
        m_geometry = {};
        m_buildInfo = {};
        m_buildScratchSize = 0;
        m_updateScratchSize = 0;
	}

	VkAccelerationStructureKHR getHandle() const { return m_blasHandle; }
	uint64_t getDeviceAddress() const { return m_deviceAddress; }
    uint64_t getPrimitiveCount() const { return m_primitiveCount; }

private:
	VkAccelerationStructureKHR m_blasHandle = VK_NULL_HANDLE;
	uint64_t m_deviceAddress = 0;
	Buffer<BufferType::DeviceLocal> m_buffer;

	uint32_t m_primitiveCount = 0;
	Buffer<BufferType::HostVisible> m_aabbBuffer;
	ScratchBuffer m_scratchBuffer;

    VkAccelerationStructureGeometryKHR m_geometry{};
    VkAccelerationStructureBuildGeometryInfoKHR m_buildInfo{};

    VkDeviceSize m_buildScratchSize = 0;
    VkDeviceSize m_updateScratchSize = 0;
};


class TLAS
{
public:
    TLAS() = default;
    ~TLAS() = default;

    TLAS(const TLAS&) = delete;
    TLAS& operator=(const TLAS&) = delete;
    TLAS(TLAS&&) = delete;
    TLAS& operator=(TLAS&&) = delete;

    void init(const VkAccelerationStructureInstanceKHR* initialInstanceData, uint32_t instanceCount, uint32_t maxInstances)
    {
        if (initialInstanceData == nullptr && instanceCount > 0) {
             throw std::runtime_error("Initial TLAS instance data cannot be null if instanceCount > 0.");
        }
        if (instanceCount > maxInstances) {
            throw std::runtime_error("Initial instance count exceeds maximum instance count.");
        }
        m_maxInstances = maxInstances;

        VkDeviceSize instanceBufferSize = sizeof(VkAccelerationStructureInstanceKHR) * m_maxInstances;
        const VkBufferUsageFlags instanceBufferUsageFlags =
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

        m_instanceBuffer.create(
            VulkanContext::vmaAllocator,
            VulkanContext::device,
            instanceBufferSize,
            instanceBufferUsageFlags,
            VulkanContext::vkGetBufferDeviceAddressKHR
        );

        VkDeviceOrHostAddressConstKHR instanceDataDeviceAddress{};
        instanceDataDeviceAddress.deviceAddress = m_instanceBuffer.deviceAddress;

        m_geometry = {};
        m_geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        m_geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        m_geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
        m_geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        m_geometry.geometry.instances.arrayOfPointers = VK_FALSE;
        m_geometry.geometry.instances.data = instanceDataDeviceAddress;

        m_buildInfo = {};
        m_buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        m_buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        m_buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
        m_buildInfo.geometryCount = 1;
        m_buildInfo.pGeometries = &m_geometry;

        uint32_t maxPrimitiveCount = m_maxInstances;
        VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo{};
        buildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        VulkanContext::vkGetAccelerationStructureBuildSizesKHR(
            VulkanContext::device,
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &m_buildInfo,
            &maxPrimitiveCount,
            &buildSizesInfo);

        if (buildSizesInfo.accelerationStructureSize == 0) {
            throw std::runtime_error("TLAS size query returned 0.");
        }
        m_buildScratchSize = buildSizesInfo.buildScratchSize;
        m_updateScratchSize = buildSizesInfo.updateScratchSize;

        m_buffer.create(
            VulkanContext::vmaAllocator,
            VulkanContext::device,
            buildSizesInfo.accelerationStructureSize,
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VulkanContext::vkGetBufferDeviceAddressKHR
        );

        VkAccelerationStructureCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        createInfo.buffer = m_buffer.handle;
        createInfo.size = buildSizesInfo.accelerationStructureSize;
        createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

        VkResult result = VulkanContext::vkCreateAccelerationStructureKHR(
            VulkanContext::device,
            &createInfo,
            nullptr,
            &m_tlasHandle);
        if (result != VK_SUCCESS) {
             throw std::runtime_error("Failed to create TLAS handle.");
        }

        VkDeviceSize scratchSize = std::max(m_buildScratchSize, m_updateScratchSize);
        m_scratchBuffer.createScratchBuffer(
            VulkanContext::vmaAllocator,
            VulkanContext::device,
            scratchSize,
            VulkanContext::vkGetBufferDeviceAddressKHR
        );

        VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
        addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        addressInfo.accelerationStructure = m_tlasHandle;
        m_deviceAddress = VulkanContext::vkGetAccelerationStructureDeviceAddressKHR(VulkanContext::device, &addressInfo);

        if (instanceCount > 0) 
        {
            VkCommandBuffer cmd = VulkanContext::CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
            build(cmd, initialInstanceData, instanceCount, false);
            VulkanContext::SubmitCommandBuffer(cmd, VulkanContext::graphicsQueue, true);
        }
        else 
        {
            m_instanceCount = 0;
        }
    }

    void build(VkCommandBuffer cmd, 
               const VkAccelerationStructureInstanceKHR* instanceData,
               uint32_t instanceCount,
               bool update = false)
    {
        if (m_tlasHandle == VK_NULL_HANDLE) 
        {
            throw std::runtime_error("TLAS must be initialized before building.");
        }
        if (instanceData == nullptr && instanceCount > 0) 
        {
            throw std::runtime_error("Instance data cannot be null if instanceCount > 0 for TLAS build.");
        }
        if (instanceCount > m_maxInstances) 
        {
            throw std::runtime_error("Instance count exceeds maximum instance count during build.");
        }

        bool performUpdate = update;
        if (performUpdate && m_buildInfo.srcAccelerationStructure == VK_NULL_HANDLE) 
        {
            performUpdate = false; 
        }

        VkDeviceSize dataSize = sizeof(VkAccelerationStructureInstanceKHR) * instanceCount;
        if (dataSize > 0) 
        {
            m_instanceBuffer.updateData(VulkanContext::vmaAllocator, instanceData, dataSize);
        }

        m_instanceCount = instanceCount;

        VkAccelerationStructureBuildGeometryInfoKHR buildInfo = m_buildInfo;
        buildInfo.mode = performUpdate ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        buildInfo.dstAccelerationStructure = m_tlasHandle;
        buildInfo.scratchData.deviceAddress = m_scratchBuffer.deviceAddress;
        buildInfo.srcAccelerationStructure = performUpdate ? m_tlasHandle : VK_NULL_HANDLE;
        buildInfo.pGeometries = &m_geometry;

        VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
        buildRangeInfo.primitiveCount = m_instanceCount;
        buildRangeInfo.primitiveOffset = 0;
        buildRangeInfo.firstVertex = 0;
        buildRangeInfo.transformOffset = 0;

        const VkAccelerationStructureBuildRangeInfoKHR* pBuildRangeInfo = &buildRangeInfo;


        VkMemoryBarrier memoryBarrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
        memoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_HOST_BIT,
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            0,
            1, &memoryBarrier,
            0, nullptr,
            0, nullptr
        );

        VulkanContext::vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pBuildRangeInfo);

        memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
            0,
            1, &memoryBarrier,
            0, nullptr,
            0, nullptr
        );

        if (!performUpdate) 
        {
            m_buildInfo.srcAccelerationStructure = m_tlasHandle;
        }
    }

    void destroy()
    {
        if (m_tlasHandle != VK_NULL_HANDLE) 
        {
            VulkanContext::vkDestroyAccelerationStructureKHR(VulkanContext::device, m_tlasHandle, nullptr);
            m_tlasHandle = VK_NULL_HANDLE;
        }
        m_buffer.destroy(VulkanContext::vmaAllocator);
        m_instanceBuffer.destroy(VulkanContext::vmaAllocator);
        m_scratchBuffer.destroyScratchBuffer(VulkanContext::vmaAllocator);

        m_deviceAddress = 0;
        m_instanceCount = 0;
        m_maxInstances = 0;
        m_geometry = {};
        m_buildInfo = {};
        m_buildScratchSize = 0;
        m_updateScratchSize = 0;
    }

    VkAccelerationStructureKHR getHandle() const { return m_tlasHandle; }
    uint64_t getDeviceAddress() const { return m_deviceAddress; }
    uint32_t getCurrentInstanceCount() const { return m_instanceCount; }
    uint32_t getMaxInstanceCount() const { return m_maxInstances; }

    Buffer<BufferType::HostVisible>& getInstanceBuffer() { return m_instanceBuffer; }
    Buffer<BufferType::DeviceLocal>& getBuffer() { return m_buffer; }

private:
    VkAccelerationStructureKHR m_tlasHandle = VK_NULL_HANDLE;
    uint64_t m_deviceAddress = 0;
    Buffer<BufferType::DeviceLocal> m_buffer;
    Buffer<BufferType::HostVisible> m_instanceBuffer;
    ScratchBuffer m_scratchBuffer;

    uint32_t m_instanceCount = 0;
    uint32_t m_maxInstances = 0;

    VkAccelerationStructureGeometryKHR m_geometry{};
    VkAccelerationStructureBuildGeometryInfoKHR m_buildInfo{};

    VkDeviceSize m_buildScratchSize = 0;
    VkDeviceSize m_updateScratchSize = 0;
};


class AccelerationStructureManager
{
public:
    AccelerationStructureManager() = default;
    ~AccelerationStructureManager() = default;

    AccelerationStructureManager(const AccelerationStructureManager&) = delete;
    AccelerationStructureManager& operator=(const AccelerationStructureManager&) = delete;
    AccelerationStructureManager(AccelerationStructureManager&&) = delete;
    AccelerationStructureManager& operator=(AccelerationStructureManager&&) = delete;

    uint32_t addBLAS(const VkAabbPositionsKHR* initialAABBData, uint32_t aabbDataSize);

    void initTLAS();

    void moveBLAS(uint32_t index, VkTransformMatrixKHR transform);
    void updateTLAS(VkCommandBuffer cmd);

    void destroy()
    {
        for (auto& blas : m_blases) 
        {
			blas.destroy();
		}

		m_blases.clear();
		m_transformMatrices.clear();

		m_tlas.destroy();
    }

    VkAccelerationStructureKHR getTLASHandle() const { return m_tlas.getHandle(); }


private:
    std::vector<BLAS> m_blases;
    std::vector<VkTransformMatrixKHR> m_transformMatrices;

    TLAS m_tlas; 
};


inline uint32_t AccelerationStructureManager::addBLAS(const VkAabbPositionsKHR* initialAABBData, uint32_t aabbDataSize) 
{
		m_blases.emplace_back();
        try 
        {
            m_blases.back().init(initialAABBData, aabbDataSize);
        } 
        catch (...) 
        {
            m_blases.pop_back();
            throw;
        }

        m_transformMatrices.emplace_back();
        auto& matrix = m_transformMatrices.back();
        memset(&matrix, 0, sizeof(VkTransformMatrixKHR));
        matrix.matrix[0][0] = 1.0f;
        matrix.matrix[1][1] = 1.0f;
        matrix.matrix[2][2] = 1.0f;
        return static_cast<uint32_t>(m_blases.size() - 1);
}

inline void AccelerationStructureManager::initTLAS() 
{
    std::vector<VkAccelerationStructureInstanceKHR> instances;

    uint64_t primitiveCount = 0;

    for (size_t i = 0; i < m_blases.size(); ++i) 
    {
        VkAccelerationStructureInstanceKHR instance{};
        instance.transform = m_transformMatrices[i];
        
        instance.instanceCustomIndex = static_cast<uint32_t>(primitiveCount);
        instance.mask = 0xFF;
        instance.instanceShaderBindingTableRecordOffset = 0;
        instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        instance.accelerationStructureReference = m_blases[i].getDeviceAddress();
        instances.push_back(instance);
        primitiveCount += m_blases[i].getPrimitiveCount();
    }

    if (!instances.empty()) 
    {
        m_tlas.init(instances.data(), static_cast<uint32_t>(instances.size()), static_cast<uint32_t>(instances.size()));
    } 
    else 
    {
        m_tlas.init(nullptr, 0, 0);
    }
}

inline void AccelerationStructureManager::moveBLAS(uint32_t index, VkTransformMatrixKHR vkTransform) 
{
    if (index >= m_blases.size()) {
        throw std::runtime_error("MoveBLAS: BLAS index out of range.");
    }

    m_transformMatrices[index] = vkTransform;

    size_t instanceIndex = index;
    if (instanceIndex >= m_tlas.getMaxInstanceCount()) 
    {
        throw std::runtime_error("moveBLAS: Instance index exceeds TLAS capacity.");
    }

    VkAccelerationStructureInstanceKHR* instances = static_cast<VkAccelerationStructureInstanceKHR*>(m_tlas.getInstanceBuffer().getMappedMemory());
    instances[instanceIndex].transform = vkTransform;
}

inline void AccelerationStructureManager::updateTLAS(VkCommandBuffer cmd)
{
    PERF_SCOPE("Move BLAS");
    VkAccelerationStructureInstanceKHR* instances = static_cast<VkAccelerationStructureInstanceKHR*>(m_tlas.getInstanceBuffer().getMappedMemory());
    m_tlas.build(cmd, instances, m_tlas.getCurrentInstanceCount(), true);
}

