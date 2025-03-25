#pragma once

#include "vulkan/vulkan.h"
#include "MemoryClasses.hpp"

enum class BlasType
{
    Static,
    Dynamic,
};

struct AccelerationStructure
{
    VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
    uint64_t deviceAddress;
    ManagedBuffer buffer;
};

template <BlasType Type>
class BLAS
{
public:
    void init();
    void rebuild();
    void update();

    VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
    uint64_t deviceAddress;
    ManagedBuffer buffer;
    ScratchBuffer scratchBuffer;  
    

private:
    uint32_t updateCount = 0;
    static constexpr uint32_t MAX_UPDATES_BEFORE_REBUILD = 64;

    struct Geometry
    {

    };
};



class AcclerationStructureManager
{

};