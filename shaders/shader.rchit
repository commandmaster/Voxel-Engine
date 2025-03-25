#version 460
#extension GL_EXT_ray_tracing : require

struct RayPayload 
{
    vec3 color;
    float distance;
    vec3 normal;
    uint seed;
    vec4 debugColor;
};

struct HitInformation
{
    vec3 normal;
    bool debugHit;
};


layout(location = 0) rayPayloadInEXT RayPayload payload;
hitAttributeEXT HitInformation attribs;

layout(push_constant) uniform Constants {
    vec3 lightDir; // Direction to the light source (should be normalized)
    vec3 lightColor; // Color of the light
    vec3 ambientColor; // Ambient light color
} constants;

void main() {
    
    if (!attribs.debugHit) 
    {
		// Surface normal comes from the hit attributes
		vec3 normal = attribs.normal;
		
		// Calculate diffuse lighting - dot product of normal and light direction
		float diffuse = max(dot(normal, normalize(constants.lightDir)), 0.0);
		
		// Base material color (could be added as a parameter per sphere)
		vec3 materialColor = vec3(0.8, 0.2, 0.2); // Red material
		
		// Combine ambient and diffuse components
		vec3 ambient = constants.ambientColor * materialColor;
		vec3 diffuseColor = constants.lightColor * materialColor * diffuse;
		
		// Final color is the sum of ambient and diffuse
		payload.color = ambient + diffuseColor;
		payload.normal = normal;


	}
	else
	{
		payload.debugColor = vec4(1.0, 0.0, 0.0, 1);
	}


}