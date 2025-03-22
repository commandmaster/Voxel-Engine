#version 460
#extension GL_EXT_ray_tracing : require

struct RayPayload 
{
    vec3 color;
    float distance;
    vec3 normal;
    uint seed;
};

layout(location = 0) rayPayloadInEXT RayPayload payload;

void main() {
    payload.color = vec3(0.0, 0.5, 1.0); // Cyan color for background
}