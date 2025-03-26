#include "vulkan/vulkan.h"
#include <iostream>
#include <stdexcept>

inline std::string vkResultToString(VkResult result) 
{
    switch (result) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
        case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
        case VK_ERROR_FRAGMENTATION: return "VK_ERROR_FRAGMENTATION";
        case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
        case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT: return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
        case VK_ERROR_NOT_PERMITTED_EXT: return "VK_ERROR_NOT_PERMITTED_EXT";
        
        default: return "UNKNOWN_ERROR";
    }
}

#define LOG_ERROR_MODE 3
#define LOG_VERBOSE_MODE 2
#define LOG_NORMAL_MODE 1
#define LOG_NONE 0

#define LOG_MODE LOG_ERROR_MODE

#if LOG_MODE <= LOG_ERROR_MODE
#define LOG_ERROR(x) \
    do { \
        std::cerr << x << std::endl; \
    } while (0)
#define VK_ERROR_CHECK(x) \
    do { \
        VkResult err = x; \
        if (err != VK_SUCCESS) { \
            LOG_ERROR("Vulkan error: " + vkResultToString(err)); \
        } \
    } while (0)
#else
#define LOG_ERROR(x) x
#define VK_ERROR_CHECK(x) x
#endif


#if LOG_MODE <= LOG_VERBOSE_MODE
#define LOG_VERBOSE(x) \
    do { \
        std::cout << x << std::endl; \
    } while (0)
#else
#define LOG_VERBOSE(x)
#endif


#if LOG_MODE <= LOG_NORMAL_MODE
#define LOG_NORMAL(x) \
    do { \
        std::cout << x << std::endl; \
    } while (0)
#else
#define LOG_NORMAL(x)
#endif
