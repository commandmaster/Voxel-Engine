#include "Chunk.h"

void VoxelDataPool::init(uint32_t maxChunks)
{
    if (maxChunks == 0)
    {
        throw std::runtime_error("ChunkPool maxChunks cannot be zero.");
    }
    m_maxChunks = maxChunks;

    VkDeviceSize totalBufferSize = static_cast<VkDeviceSize>(maxChunks) * CHUNK_SIZE_BYTES;

    voxelsBuffer.create(
        VulkanContext::vmaAllocator,
        VulkanContext::device,
        totalBufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VulkanContext::vkGetBufferDeviceAddressKHR,
        (totalBufferSize > 256 * 1024 * 1024)
    );

    for (uint32_t i = 0; i < maxChunks; ++i)
    {
        m_freeSlots.push(maxChunks - 1 - i);
    }
    m_currentChunkCount = 0;
}

void VoxelDataPool::destroy()
{
    voxelsBuffer.destroy(VulkanContext::vmaAllocator);
    m_maxChunks = 0;
    m_currentChunkCount = 0;
    std::stack<uint32_t>().swap(m_freeSlots);
}

uint32_t VoxelDataPool::allocateChunkSlot()
{
    if (m_freeSlots.empty())
    {
        LOG_ERROR("ChunkPool::allocateChunkSlot - Pool is full!");
        return INVALID_CHUNK_INDEX;
    }

    uint32_t index = m_freeSlots.top();
    m_freeSlots.pop();
    m_currentChunkCount++;
    return index;
}

inline void VoxelDataPool::freeChunkSlot(uint32_t index)
{
    if (index >= m_maxChunks)
    {
        LOG_ERROR("ChunkPool::freeChunkSlot - Invalid index provided!");
        return;
    }
    // TODO: Add check to ensure we aren't double-freeing?
    // Could use a std::vector<bool> m_isSlotAllocated; alongside the stack.

    m_freeSlots.push(index);
    m_currentChunkCount--;
}

void VoxelDataPool::uploadChunkData(uint32_t voxelDataPoolIndex, const VoxelID* voxelData)
{
    if (voxelDataPoolIndex == INVALID_CHUNK_INDEX || voxelDataPoolIndex >= m_maxChunks)
    {
        LOG_ERROR("ChunkPool::uploadChunkData - Invalid chunk pool index!");
        return;
    }
    if (voxelData == nullptr)
    {
        LOG_ERROR("ChunkPool::uploadChunkData - Voxel data is null!");
        return;
    }

    VkDeviceSize offset = static_cast<VkDeviceSize>(voxelDataPoolIndex) * CHUNK_SIZE_BYTES;
    VkDeviceSize dataSize = CHUNK_VOLUME * sizeof(VoxelID);

    voxelsBuffer.uploadData(
        VulkanContext::vmaAllocator,
        VulkanContext::device,
        VulkanContext::graphicsQueue,
        voxelData,
        dataSize,
        offset
    );
}

void ChunkASManager::init(uint32_t maxChunks)
{
    if (maxChunks == 0)
    {
        throw std::runtime_error("ChunkASManager maxChunks cannot be zero.");
    }
    m_maxInstances = maxChunks;

    m_blasPool.resize(m_maxInstances);
    for (uint32_t i = 0; i < m_maxInstances; ++i)
    {
        m_freeBlasSlots.push(m_maxInstances - 1 - i);
    }

    m_tlas.init(nullptr, 0, m_maxInstances);

    m_tlasInstancesData.reserve(m_maxInstances);
    m_chunkToInstanceMap.reserve(m_maxInstances);
    m_instanceIndexToChunkMap.resize(m_maxInstances, nullptr);

    m_tlasNeedsRebuild = true;
    m_tlasBuiltOnce = false;
}

void ChunkASManager::destroy()
{
    for (size_t i = 0; i < m_blasPool.size(); ++i)
    {
        m_blasPool[i].destroy();
    }
    m_blasPool.clear();

    std::stack<BlasPoolIndex>().swap(m_freeBlasSlots);

    m_tlas.destroy();

    m_tlasInstancesData.clear();
    m_chunkToInstanceMap.clear();
    m_instanceIndexToChunkMap.clear();

    m_maxInstances = 0;
}

BlasPoolIndex ChunkASManager::allocateAndBuildBlas(Chunk* ownerChunk, uint32_t voxelDataPoolIndex, const VkAabbPositionsKHR* aabbData, uint32_t aabbDataSize)
{
    if (!ownerChunk) throw std::runtime_error("allocateAndBuildBlas: ownerChunk cannot be null.");
    if (m_freeBlasSlots.empty()) throw std::runtime_error("allocateAndBuildBlas: BLAS pool is full.");
    if (m_tlasInstancesData.size() >= m_maxInstances) throw std::runtime_error("allocateAndBuildBlas: TLAS instance capacity reached.");

    BlasPoolIndex blasPoolIdx = m_freeBlasSlots.top();
    m_freeBlasSlots.pop();

    BLAS& blas = m_blasPool[blasPoolIdx];
    try
    {
        blas.init(aabbData, aabbDataSize);
    }
    catch (...)
    {
        m_freeBlasSlots.push(blasPoolIdx);
        throw;
    }

    TlasInstanceIndex tlasInstanceIdx = static_cast<TlasInstanceIndex>(m_tlasInstancesData.size());

    VkAccelerationStructureInstanceKHR instance{};
    instance.transform = ownerChunk->transform;
    instance.instanceCustomIndex = voxelDataPoolIndex;
    instance.mask = 0xFF;
    instance.instanceShaderBindingTableRecordOffset = 0;
    instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    instance.accelerationStructureReference = blas.getDeviceAddress();

    m_tlasInstancesData.push_back(instance);

    m_chunkToInstanceMap[ownerChunk] = tlasInstanceIdx;
    m_instanceIndexToChunkMap[tlasInstanceIdx] = ownerChunk;

    ownerChunk->blasPoolIndex = blasPoolIdx;
    ownerChunk->tlasInstanceIndex = tlasInstanceIdx;

    m_tlasNeedsRebuild = true;

    return blasPoolIdx;
}

void ChunkASManager::releaseBlas(Chunk* ownerChunk)
{
    if (!ownerChunk) 
    {
        LOG_ERROR("releaseBlas: ownerChunk cannot be null.");
        return;
    }

    BlasPoolIndex blasPoolIdx = ownerChunk->blasPoolIndex;
    TlasInstanceIndex tlasInstanceIdx = ownerChunk->tlasInstanceIndex;

    if (blasPoolIdx == INVALID_BLAS_POOL_INDEX || tlasInstanceIdx == INVALID_TLAS_INSTANCE_INDEX)
    {
        LOG_ERROR("releaseBlas: Chunk does not have valid AS indices.");
        return;
    }

    if (tlasInstanceIdx >= m_tlasInstancesData.size() || m_instanceIndexToChunkMap[tlasInstanceIdx] != ownerChunk)
    {
        LOG_ERROR("releaseBlas: TLAS instance index mismatch or out of bounds.");

        auto it = m_chunkToInstanceMap.find(ownerChunk);
        if (it != m_chunkToInstanceMap.end())
        {
            tlasInstanceIdx = it->second;
            LOG_WARNING("releaseBlas: Found correct TLAS index via map lookup.");
        }
        else
        {
            LOG_ERROR("releaseBlas: Cannot find TLAS instance for chunk via map either.");
            ownerChunk->blasPoolIndex = INVALID_BLAS_POOL_INDEX;
            ownerChunk->tlasInstanceIndex = INVALID_TLAS_INSTANCE_INDEX;
            return;
        }
    }


    m_blasPool[blasPoolIdx].destroy();

    m_freeBlasSlots.push(blasPoolIdx);

    TlasInstanceIndex lastElementIndex = static_cast<TlasInstanceIndex>(m_tlasInstancesData.size() - 1);
    Chunk* chunkBeingMoved = m_instanceIndexToChunkMap[lastElementIndex]; // Chunk owning the last instance

    if (tlasInstanceIdx != lastElementIndex)
    {
        std::swap(m_tlasInstancesData[tlasInstanceIdx], m_tlasInstancesData[lastElementIndex]);

        m_instanceIndexToChunkMap[tlasInstanceIdx] = chunkBeingMoved;
        m_chunkToInstanceMap[chunkBeingMoved] = tlasInstanceIdx; // Update its instance index

        if (chunkBeingMoved)
        {
            chunkBeingMoved->tlasInstanceIndex = tlasInstanceIdx;
        }
        else
        {
            LOG_ERROR("releaseBlas: Chunk owning the last instance was null during swap!");
        }
    }

    m_tlasInstancesData.pop_back();

    m_instanceIndexToChunkMap[lastElementIndex] = nullptr; // Clear the entry for the old last index
    m_chunkToInstanceMap.erase(ownerChunk);

    ownerChunk->blasPoolIndex = INVALID_BLAS_POOL_INDEX;
    ownerChunk->tlasInstanceIndex = INVALID_TLAS_INSTANCE_INDEX;

    m_tlasNeedsRebuild = true;
}

void ChunkASManager::updateChunkTransform(Chunk* ownerChunk) {
    if (!ownerChunk) {
        LOG_ERROR("updateChunkTransform: ownerChunk cannot be null.");
        return;
    }

    TlasInstanceIndex tlasInstanceIdx = ownerChunk->tlasInstanceIndex;

    auto it = m_chunkToInstanceMap.find(ownerChunk);
    if (it != m_chunkToInstanceMap.end())
    {
        tlasInstanceIdx = it->second;
    }
    else
    {
        LOG_ERROR("updateChunkTransform: Chunk not found in instance map.");
        return; // Or use chunk's index if you trust it more
    }


    if (tlasInstanceIdx == INVALID_TLAS_INSTANCE_INDEX || tlasInstanceIdx >= m_tlasInstancesData.size()) {
        LOG_ERROR("updateChunkTransform: Invalid TLAS instance index for chunk.");
        return;
    }

    m_tlasInstancesData[tlasInstanceIdx].transform = ownerChunk->transform;
    m_tlasNeedsRebuild = true; // Moving requires a TLAS update/rebuild
}

void ChunkASManager::buildOrUpdateTLAS(VkCommandBuffer cmd)
{
    if (!m_tlasNeedsRebuild && m_tlasBuiltOnce) 
    {
        return;
    }
    if (m_tlasInstancesData.empty() && m_tlasBuiltOnce) 
    {
        LOG_WARNING("buildOrUpdateTLAS: Attempting to build TLAS with zero instances.");
    }

    PERF_SCOPE("Build/Update TLAS");

    bool performUpdate = m_tlasBuiltOnce;

    m_tlas.build(
        cmd,
        m_tlasInstancesData.data(),
        static_cast<uint32_t>(m_tlasInstancesData.size()),
        performUpdate
    );

    m_tlasNeedsRebuild = false;
    if (!m_tlasInstancesData.empty())
    {
        m_tlasBuiltOnce = true;
    }
}
