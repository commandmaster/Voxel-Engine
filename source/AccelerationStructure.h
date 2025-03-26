#pragma once

#include "vulkan/vulkan.h"
#include "Buffer.hpp"
#include <vector>

enum class BlasType
{
    Static,
    Dynamic,
};

struct Geometry
{
    std::vector<VkAabbPositionsKHR> aabbs;
    Buffer<BufferType::HostVisible> aabbBuffer;
};

template <BlasType Type>
class BLAS
{
public:
    BLAS() = default;
    ~BLAS();

    void init(const std::vector<VkAabbPositionsKHR>& aabbs);
    void cleanup();
    void rebuild(const std::vector<VkAabbPositionsKHR>& aabbs);
    
private:
    VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
    uint64_t deviceAddress;
    Buffer<BufferType::DeviceLocal> accelerationStructureStorage;
    ScratchBuffer scratchBuffer;  

    Geometry geometry;
};

class AccelerationStructureManager
{
public:
    AccelerationStructureManager() = default;
    ~AccelerationStructureManager() = default;
    
    void init(VkDevice device, VmaAllocator allocator);
    void cleanup();
    
private:
    VkDevice device = VK_NULL_HANDLE;
    VmaAllocator allocator = VK_NULL_HANDLE;
};

template<>
class BLAS<BlasType::Static>
{
public:
    BLAS() = default;
    ~BLAS() = default;
    
    void init(const std::vector<VkAabbPositionsKHR>& aabbs);
    void cleanup();
    void rebuild(const std::vector<VkAabbPositionsKHR>& aabbs);

private:
    VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
    uint64_t deviceAddress;
    Buffer<BufferType::DeviceLocal> accelerationStructureStorage;
    ScratchBuffer scratchBuffer;  

    Geometry geometry;
};

template<>
class BLAS<BlasType::Dynamic>
{
public:
    BLAS() = default;
    ~BLAS() = default;

    void init(const std::vector<VkAabbPositionsKHR>& aabbs);
    void cleanup();
    void update();
    void rebuild(const std::vector<VkAabbPositionsKHR>& aabbs);

private:
    VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
    uint64_t deviceAddress;
    Buffer<BufferType::DeviceLocal> accelerationStructureStorage;
    ScratchBuffer scratchBuffer;  

    Geometry geometry;
};








