//#pragma once
//
//#include "PerformanceTimer.hpp"
//#include "DebugUtils.hpp"
//#include "VulkanContext.hpp"
//#include "AccelerationStructure.h"
//#include "Buffer.hpp"
//
//#include <cstdint>
//#include <vector>
//#include <stack>
//#include <unordered_map>
//#include <stdexcept>
//
//#include "glm/gtc/matrix_transform.hpp"
//
//using VoxelID = uint8_t;
//
//constexpr uint32_t CHUNK_SIZE_X = 32;
//constexpr uint32_t CHUNK_SIZE_Y = 32;
//constexpr uint32_t CHUNK_SIZE_Z = 32;
//constexpr uint32_t CHUNK_VOLUME = CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z;
//constexpr uint32_t CHUNK_SIZE_BYTES = CHUNK_VOLUME * sizeof(VoxelID);
//
//
//struct MaterialEntry
//{
//    glm::vec4 albedo;
//    glm::vec4 roughness;
//    glm::vec4 metallic;
//};
//
//constexpr uint32_t INVALID_POOL_INDEX = 0xFFFFFFFF;
//
//class VoxelDataPool
//{
//public:
//    VoxelDataPool() = default;
//    ~VoxelDataPool() = default;
//
//    VoxelDataPool(const VoxelDataPool&) = delete;
//    VoxelDataPool& operator=(const VoxelDataPool&) = delete;
//    VoxelDataPool(VoxelDataPool&&) = delete;
//    VoxelDataPool& operator=(VoxelDataPool&&) = delete;
//
//    void init(uint32_t maxChunks);
//
//    void destroy();
//
//    uint32_t allocateChunkSlot();
//
//    void freeChunkSlot(uint32_t index);
//
//    void uploadChunkData(uint32_t voxelDataPoolIndex, const VoxelID* voxelData);
//
//    VkBuffer getVoxelsBufferHandle() const { return voxelsBuffer.handle; }
//    uint64_t getVoxelsBufferDeviceAddress() const { return voxelsBuffer.deviceAddress; }
//    uint32_t getCurrentChunkCount() const { return m_currentChunkCount; }
//    uint32_t getMaxChunks() const { return m_maxChunks; }
//
//
//private:
//    Buffer<BufferType::DeviceLocal> voxelsBuffer;
//    uint32_t m_maxChunks = 0;
//    uint32_t m_currentChunkCount = 0;
//
//    std::stack<uint32_t> m_freeSlots;
//};
//
//using BlasPoolIndex = uint32_t;
//using TlasInstanceIndex = uint32_t;
//constexpr TlasInstanceIndex INVALID_TLAS_INSTANCE_INDEX = 0xFFFFFFFF;
//
//struct Chunk;
//class ChunkASManager
//{
//public:
//    ChunkASManager() = default;
//    ~ChunkASManager() = default;
//
//    ChunkASManager(const ChunkASManager&) = delete;
//    ChunkASManager& operator=(const ChunkASManager&) = delete;
//    ChunkASManager(ChunkASManager&&) = delete;
//    ChunkASManager& operator=(ChunkASManager&&) = delete;
//
//    void init(uint32_t maxChunks);
//
//    void destroy();
//
//    TlasInstanceIndex addChunkInstance(Chunk* ownerChunk, uint32_t voxelDataPoolIndex);
//
//    bool removeChunkInstance(Chunk* ownerChunk);
//
//    void updateChunkTransform(Chunk* ownerChunk);
//
//    void recordUpdateCommands(VkCommandBuffer cmd);
//
//    VkAccelerationStructureKHR getTLASHandle() const { return m_tlasHandle; }
//
//
//private:
//    // Tlas Data
//    uint32_t m_maxInstances = 0;
//    uint32_t m_instanceCount = 0;
//    VkDeviceSize m_buildScratchSize = 0;
//    VkAccelerationStructureKHR m_tlasHandle = VK_NULL_HANDLE;
//    uint64_t m_deviceAddress = 0;
//    Buffer<BufferType::DeviceLocal> m_tlasStructureBuffer;
//    ScratchBuffer m_scratchBuffer;
//	VkAccelerationStructureGeometryKHR m_tlasGeometry{};
//    VkAccelerationStructureBuildGeometryInfoKHR m_tlasBuildInfo{};
//	VkDeviceSize m_buildScratchSize = 0;
//    VkDeviceSize m_updateScratchSize = 0;
//
//    BLAS m_sharedChunkBlas;
//
//    Buffer<BufferType::DeviceLocal> m_instanceBuffer;
//    std::vector<VkAccelerationStructureInstanceKHR> m_instances;
//
//    std::stack<TlasInstanceIndex> m_freeSlots;
//};
//
//
//
//struct Chunk
//{
//    TlasInstanceIndex tlasInstanceIndex = INVALID_TLAS_INSTANCE_INDEX;
//    uint32_t voxelPoolIndex = INVALID_POOL_INDEX;
//
//	VkTransformMatrixKHR transform;
//    glm::ivec3 position;
//
//    void init(glm::ivec3 position);
//    void destroy();
//};
