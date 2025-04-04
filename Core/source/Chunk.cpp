//#include "Chunk.h"
//
//void VoxelDataPool::init(uint32_t maxChunks)
//{
//    if (maxChunks == 0)
//    {
//        throw std::runtime_error("ChunkPool maxChunks cannot be zero.");
//    }
//    m_maxChunks = maxChunks;
//
//    VkDeviceSize totalBufferSize = static_cast<VkDeviceSize>(maxChunks) * CHUNK_SIZE_BYTES;
//
//    voxelsBuffer.create(
//        VulkanContext::vmaAllocator,
//        VulkanContext::device,
//        totalBufferSize,
//        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
//        VulkanContext::vkGetBufferDeviceAddressKHR,
//        (totalBufferSize > 256 * 1024 * 1024)
//    );
//
//    for (uint32_t i = 0; i < maxChunks; ++i)
//    {
//        m_freeSlots.push(maxChunks - 1 - i);
//    }
//    m_currentChunkCount = 0;
//}
//
//void VoxelDataPool::destroy()
//{
//    voxelsBuffer.destroy(VulkanContext::vmaAllocator);
//    m_maxChunks = 0;
//    m_currentChunkCount = 0;
//    std::stack<uint32_t>().swap(m_freeSlots);
//}
//
//uint32_t VoxelDataPool::allocateChunkSlot()
//{
//    if (m_freeSlots.empty())
//    {
//        LOG_ERROR("ChunkPool::allocateChunkSlot - Pool is full!");
//        return INVALID_POOL_INDEX;
//    }
//
//    uint32_t index = m_freeSlots.top();
//    m_freeSlots.pop();
//    m_currentChunkCount++;
//    return index;
//}
//
//inline void VoxelDataPool::freeChunkSlot(uint32_t index)
//{
//    if (index >= m_maxChunks)
//    {
//        LOG_ERROR("ChunkPool::freeChunkSlot - Invalid index provided!");
//        return;
//    }
//    // TODO: Add check to ensure we aren't double-freeing?
//    // Could use a std::vector<bool> m_isSlotAllocated; alongside the stack.
//
//    m_freeSlots.push(index);
//    m_currentChunkCount--;
//}
//
//void VoxelDataPool::uploadChunkData(uint32_t voxelDataPoolIndex, const VoxelID* voxelData)
//{
//    if (voxelDataPoolIndex == INVALID_POOL_INDEX || voxelDataPoolIndex >= m_maxChunks)
//    {
//        LOG_ERROR("ChunkPool::uploadChunkData - Invalid chunk pool index!");
//        return;
//    }
//    if (voxelData == nullptr)
//    {
//        LOG_ERROR("ChunkPool::uploadChunkData - Voxel data is null!");
//        return;
//    }
//
//    VkDeviceSize offset = static_cast<VkDeviceSize>(voxelDataPoolIndex) * CHUNK_SIZE_BYTES;
//    VkDeviceSize dataSize = CHUNK_VOLUME * sizeof(VoxelID);
//
//    voxelsBuffer.uploadData(
//        VulkanContext::vmaAllocator,
//        VulkanContext::device,
//        VulkanContext::graphicsQueue,
//        voxelData,
//        dataSize,
//        offset
//    );
//}
//
//void ChunkASManager::init(uint32_t maxChunks)
//{
//    if (maxChunks == 0)
//    {
//        throw std::runtime_error("ChunkASManager maxChunks cannot be zero.");
//    }
//
//    m_maxInstances = maxChunks;
//
//    m_instances.resize(maxChunks);
//    for (int i = 0; i < maxChunks; ++i)
//    {
//
//    }
//
//    VkAabbPositionsKHR chunkBoundsAABB = { 0, 0, 0, CHUNK_SIZE_X, CHUNK_SIZE_Y, CHUNK_SIZE_Z };
//    m_sharedChunkBlas.init(&chunkBoundsAABB, sizeof(chunkBoundsAABB));
//
//	VkDeviceSize instanceBufferSize = sizeof(VkAccelerationStructureInstanceKHR) * m_maxInstances;
//	const VkBufferUsageFlags instanceBufferUsageFlags =
//		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
//		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
//
//	m_instanceBuffer.create(
//		VulkanContext::vmaAllocator,
//		VulkanContext::device,
//		instanceBufferSize,
//		instanceBufferUsageFlags,
//		VulkanContext::vkGetBufferDeviceAddressKHR
//	);
//
//	VkDeviceOrHostAddressConstKHR instanceDataDeviceAddress{};
//	instanceDataDeviceAddress.deviceAddress = m_instanceBuffer.deviceAddress;
//
//    m_tlasGeometry = {};
//	m_tlasGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
//	m_tlasGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
//	m_tlasGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
//	m_tlasGeometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
//    m_tlasGeometry.geometry.instances.arrayOfPointers = VK_FALSE;
//    m_tlasGeometry.geometry.instances.data = instanceDataDeviceAddress;
//
//	m_tlasBuildInfo = {};
//	m_tlasBuildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
//	m_tlasBuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
//	m_tlasBuildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | 
//					   VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
//	m_tlasBuildInfo.geometryCount = 1;
//	m_tlasBuildInfo.pGeometries = &m_tlasGeometry;
//
//    uint32_t maxPrimitiveCount = m_maxInstances;
//
//	VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo{};
//	buildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
//	VulkanContext::vkGetAccelerationStructureBuildSizesKHR(
//		VulkanContext::device,
//		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
//		&m_buildInfo,
//		&maxPrimitiveCount,
//		&buildSizesInfo);
//
//	if (buildSizesInfo.accelerationStructureSize == 0) 
//	{
//		throw std::runtime_error("TLAS size query returned 0.");
//	}
//
//	m_buildScratchSize = buildSizesInfo.buildScratchSize;
//	m_updateScratchSize = buildSizesInfo.updateScratchSize;
//
//    m_tlasStructureBuffer.create(
//		VulkanContext::vmaAllocator,
//		VulkanContext::device,
//		buildSizesInfo.accelerationStructureSize,
//		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
//		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
//		VulkanContext::vkGetBufferDeviceAddressKHR
//    );
//
//    VkAccelerationStructureCreateInfoKHR createInfo{};
//	createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
//    createInfo.buffer = m_tlasStructureBuffer.handle;
//	createInfo.size = buildSizesInfo.accelerationStructureSize;
//	createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
//
//    VK_ERROR_CHECK(
//        VulkanContext::vkCreateAccelerationStructureKHR(
//            VulkanContext::device,
//            &createInfo,
//            nullptr,
//            &m_tlasHandle
//        )
//    );
//
//    VkDeviceSize scratchSize = std::max(m_buildScratchSize, m_updateScratchSize);
//	m_scratchBuffer.createScratchBuffer(
//		VulkanContext::vmaAllocator,
//		VulkanContext::device,
//		scratchSize,
//		VulkanContext::vkGetBufferDeviceAddressKHR
//	);
//
//    VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
//	addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
//	addressInfo.accelerationStructure = m_tlasHandle;
//	m_deviceAddress = VulkanContext::vkGetAccelerationStructureDeviceAddressKHR(VulkanContext::device, &addressInfo);
//}
//
//void ChunkASManager::destroy()
//{
//    m_sharedChunkBlas.destroy();
//    m_maxInstances = 0;
//}
//
//TlasInstanceIndex ChunkASManager::addChunkInstance(Chunk* ownerChunk, uint32_t voxelDataPoolIndex)
//{
//    if (!ownerChunk) throw std::runtime_error("allocateAndBuildBlas: ownerChunk cannot be null.");
//}
//
//bool ChunkASManager::removeChunkInstance(Chunk* ownerChunk)
//{
//    if (ownerChunk->tlasInstanceIndex == INVALID_TLAS_INSTANCE_INDEX)
//    {
//        LOG_ERROR("INVALID TLAS INDEX");
//    }
//
//
//}
//
//void ChunkASManager::updateChunkTransform(Chunk* ownerChunk) {
//    if (!ownerChunk) {
//        LOG_ERROR("updateChunkTransform: ownerChunk cannot be null.");
//        return;
//    }
//
//    TlasInstanceIndex tlasInstanceIdx = ownerChunk->tlasInstanceIndex;
//
//    auto it = m_chunkToInstanceMap.find(ownerChunk);
//    if (it != m_chunkToInstanceMap.end())
//    {
//        tlasInstanceIdx = it->second;
//    }
//    else
//    {
//        LOG_ERROR("updateChunkTransform: Chunk not found in instance map.");
//        return; // Or use chunk's index if you trust it more
//    }
//
//
//    if (tlasInstanceIdx == INVALID_TLAS_INSTANCE_INDEX || tlasInstanceIdx >= m_tlasInstancesData.size()) {
//        LOG_ERROR("updateChunkTransform: Invalid TLAS instance index for chunk.");
//        return;
//    }
//
//    m_tlasInstancesData[tlasInstanceIdx].transform = ownerChunk->transform;
//    m_tlasNeedsRebuild = true; // Moving requires a TLAS update/rebuild
//}
//
//void ChunkASManager::recordUpdateCommands(VkCommandBuffer cmd)
//{
//    bool performUpdate = m_tlasBuildInfo.srcAccelerationStructure != VK_NULL_HANDLE;
//
//	if (m_tlasHandle == VK_NULL_HANDLE)
//	{
//		throw std::runtime_error("TLAS must be initialized before building.");
//	}
//    if (m_instanceCount > m_maxInstances) 
//	{
//		throw std::runtime_error("Instance count exceeds maximum instance count during build.");
//	}
//
//    VkDeviceSize dataSize = sizeof(VkAccelerationStructureInstanceKHR) * m_instanceCount;
//	if (dataSize > 0) 
//	{
//		m_instanceBuffer.updateData(VulkanContext::vmaAllocator, instanceData, dataSize);
//	}
//
//	VkAccelerationStructureBuildGeometryInfoKHR buildInfo = m_tlasBuildInfo;
//	buildInfo.mode = performUpdate ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : 
//								  VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
//	buildInfo.dstAccelerationStructure = m_tlasHandle;
//	buildInfo.scratchData.deviceAddress = m_scratchBuffer.deviceAddress;
//	buildInfo.srcAccelerationStructure = performUpdate ? m_tlasHandle : VK_NULL_HANDLE;
//	buildInfo.pGeometries = &m_tlasGeometry;
//
//	VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
//	buildRangeInfo.primitiveCount = m_instanceCount;
//	buildRangeInfo.primitiveOffset = 0;
//	buildRangeInfo.firstVertex = 0;
//	buildRangeInfo.transformOffset = 0;
//
//	const VkAccelerationStructureBuildRangeInfoKHR* pBuildRangeInfo = &buildRangeInfo;
//
//	VkMemoryBarrier memoryBarrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
//	memoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
//	memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
//	vkCmdPipelineBarrier(
//		cmd,
//		VK_PIPELINE_STAGE_HOST_BIT,
//		VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
//		0,
//		1, &memoryBarrier,
//		0, nullptr,
//		0, nullptr
//	);
//
//	VulkanContext::vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pBuildRangeInfo);
//
//	memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
//	memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
//	vkCmdPipelineBarrier(
//		cmd,
//		VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
//		VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
//		0,
//		1, &memoryBarrier,
//		0, nullptr,
//		0, nullptr
//	);
//
//	if (!performUpdate) 
//	{
//        m_tlasBuildInfo.srcAccelerationStructure = m_tlasHandle;
//	}
//
//}
