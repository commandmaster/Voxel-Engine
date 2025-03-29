#pragma once

#include "PerformanceTimer.hpp"
#include "DebugUtils.hpp"
#include "VulkanContext.hpp"
#include "AccelerationStructure.h"
#include "Buffer.hpp"

#include <cstdint>
#include <vector>
#include <stack>
#include <unordered_map>
#include <stdexcept>

#include "glm/gtc/matrix_transform.hpp"

using VoxelID = uint8_t;

constexpr uint32_t CHUNK_SIZE_X = 32;
constexpr uint32_t CHUNK_SIZE_Y = 32;
constexpr uint32_t CHUNK_SIZE_Z = 32;
constexpr uint32_t CHUNK_VOLUME = CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z;
constexpr uint32_t CHUNK_SIZE_BYTES = CHUNK_VOLUME * sizeof(VoxelID);


struct MaterialEntry
{
    glm::vec4 albedo;
    glm::vec4 roughness;
    glm::vec4 metallic;
};

constexpr uint32_t INVALID_CHUNK_INDEX = 0xFFFFFFFF;

class VoxelDataPool
{
public:
    VoxelDataPool() = default;
    ~VoxelDataPool() = default;

    VoxelDataPool(const VoxelDataPool&) = delete;
    VoxelDataPool& operator=(const VoxelDataPool&) = delete;
    VoxelDataPool(VoxelDataPool&&) = delete;
    VoxelDataPool& operator=(VoxelDataPool&&) = delete;

    void init(uint32_t maxChunks);

    void destroy();

    uint32_t allocateChunkSlot();

    void freeChunkSlot(uint32_t index);

    void uploadChunkData(uint32_t voxelDataPoolIndex, const VoxelID* voxelData);

    VkBuffer getVoxelsBufferHandle() const { return voxelsBuffer.handle; }
    uint64_t getVoxelsBufferDeviceAddress() const { return voxelsBuffer.deviceAddress; }
    uint32_t getCurrentChunkCount() const { return m_currentChunkCount; }
    uint32_t getMaxChunks() const { return m_maxChunks; }


private:
    Buffer<BufferType::DeviceLocal> voxelsBuffer;
    uint32_t m_maxChunks = 0;
    uint32_t m_currentChunkCount = 0;

    std::stack<uint32_t> m_freeSlots;
};

using BlasPoolIndex = uint32_t;
using TlasInstanceIndex = uint32_t;
constexpr BlasPoolIndex INVALID_BLAS_POOL_INDEX = 0xFFFFFFFF;
constexpr TlasInstanceIndex INVALID_TLAS_INSTANCE_INDEX = 0xFFFFFFFF;

struct Chunk;
class ChunkASManager
{
public:
    ChunkASManager() = default;
    ~ChunkASManager() = default;

    ChunkASManager(const ChunkASManager&) = delete;
    ChunkASManager& operator=(const ChunkASManager&) = delete;
    ChunkASManager(ChunkASManager&&) = delete;
    ChunkASManager& operator=(ChunkASManager&&) = delete;

    void init(uint32_t maxChunks);

    void destroy();

    BlasPoolIndex allocateAndBuildBlas(
        Chunk* ownerChunk, 
        uint32_t voxelDataPoolIndex, 
        const VkAabbPositionsKHR* aabbData,
        uint32_t aabbDataSize);

    void releaseBlas(Chunk* ownerChunk);

    void updateChunkTransform(Chunk* ownerChunk);

    void buildOrUpdateTLAS(VkCommandBuffer cmd);

    VkAccelerationStructureKHR getTLASHandle() const { return m_tlas.m_tlasHandle; }
    uint32_t getActiveInstanceCount() const { return static_cast<uint32_t>(m_tlasInstancesData.size()); }


private:
    uint32_t m_maxInstances = 0;

    std::vector<BLAS> m_blasPool;
    std::stack<BlasPoolIndex> m_freeBlasSlots;

    TLAS m_tlas;

    std::vector<VkAccelerationStructureInstanceKHR> m_tlasInstancesData;

    std::unordered_map<Chunk*, TlasInstanceIndex> m_chunkToInstanceMap;
    std::vector<Chunk*> m_instanceIndexToChunkMap; 

    bool m_tlasNeedsRebuild = true;
    bool m_tlasBuiltOnce = false;
};



struct Chunk
{
    BlasPoolIndex blasPoolIndex = INVALID_BLAS_POOL_INDEX;
    TlasInstanceIndex tlasInstanceIndex = INVALID_TLAS_INSTANCE_INDEX;
    uint32_t chunkPoolIndex = INVALID_CHUNK_INDEX;

	VkTransformMatrixKHR transform;
    glm::ivec3 position;

    void init(glm::ivec3 position);
    void destroy();
};
