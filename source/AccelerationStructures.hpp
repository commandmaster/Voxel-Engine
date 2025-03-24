#pragma once

#include <vulkan/vulkan.h>
#include "vma/vk_mem_alloc.h"
#include "MemoryClasses.hpp"

#include <vector>
#include <glm/glm.hpp>

class AccelerationStructure
{
public:
    AccelerationStructure() = default;
    virtual ~AccelerationStructure() = default;

    VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
    uint64_t deviceAddress = 0;
    ManagedBuffer buffer;

    virtual void cleanup(VkDevice device, VmaAllocator allocator) = 0;
};

enum class BLASType
{
    STATIC,  // Optimized for trace performance, compacted, rarely changes
    DYNAMIC  // Optimized for frequent updates, used for moving objects
};

class BottomLevelAccelerationStructure : public AccelerationStructure
{
public:
    BottomLevelAccelerationStructure() = default;
    ~BottomLevelAccelerationStructure() override = default;

    void initialize(VkDevice device, VmaAllocator allocator, VkCommandPool commandPool, VkQueue queue, BLASType type = BLASType::STATIC);
    void build(VkDevice device, VmaAllocator allocator, VkCommandPool commandPool, VkQueue queue);
    void update(VkDevice device, VmaAllocator allocator, VkCommandPool commandPool, VkQueue queue);
    void cleanup(VkDevice device, VmaAllocator allocator) override;
    
    // Returns true if the structure should be rebuilt instead of updated
    bool shouldRebuild() const;

    struct AABBData
    {
        glm::vec3 min;
        glm::vec3 max;
    };

    struct GeometryData
    {
        ManagedBuffer aabbBuffer;
        uint32_t aabbCount = 0;
    };

    // Add AABBs for voxel chunks
    void addVoxelChunk(const std::vector<AABBData>& aabbs, VkDevice device, VmaAllocator allocator);
    
    // Add a single AABB for a dynamic object
    void addDynamicObject(const AABBData& aabb, VkDevice device, VmaAllocator allocator);
    
    // Update existing AABBs (for dynamic objects)
    void updateAABBs(uint32_t firstAABB, const std::vector<AABBData>& aabbs, VkDevice device, VmaAllocator allocator);
    
    // Legacy methods for triangle geometry
    void addGeometry(const GeometryData& geometryData);
    void addPrimitive(const void* vertices, uint32_t vertexCount, VkDeviceSize vertexStride, 
                      const uint32_t* indices, uint32_t indexCount, 
                      VkDevice device, VmaAllocator allocator);

    BLASType getType() const { return blasType; }

private:
    std::vector<VkAccelerationStructureGeometryKHR> geometries;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> buildRangeInfos;
    std::vector<GeometryData> geometryData;
    BLASType blasType = BLASType::STATIC;
    bool dirty = false;
    uint32_t updateCount = 0;
    static constexpr uint32_t MAX_UPDATES_BEFORE_REBUILD = 10; // For dynamic objects
};

class TopLevelAccelerationStructure : public AccelerationStructure
{
public:
    TopLevelAccelerationStructure() = default;
    ~TopLevelAccelerationStructure() override = default;

    void initialize(VkDevice device, VmaAllocator allocator, VkCommandPool commandPool, VkQueue queue);
    void build(VkDevice device, VmaAllocator allocator, VkCommandPool commandPool, VkQueue queue);
    void update(VkDevice device, VmaAllocator allocator, VkCommandPool commandPool, VkQueue queue);
    void cleanup(VkDevice device, VmaAllocator allocator) override;

    struct Instance
    {
        glm::mat4 transform;
        uint32_t instanceId;
        uint32_t hitGroupId;
        uint32_t mask;
        VkGeometryInstanceFlagsKHR flags;
        uint64_t accelerationStructureReference;
    };

    void addInstance(const Instance& instance);
    void updateInstance(uint32_t index, const Instance& instance);
    void removeInstance(uint32_t index);

private:
    std::vector<Instance> instances;
    ManagedBuffer instanceBuffer;
    bool dirty = false;
}; 