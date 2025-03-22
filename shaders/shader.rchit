#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec3 payload;
hitAttributeEXT vec3 attribs;

layout(push_constant) uniform Constants {
    vec3 lightDir; // Direction to the light source (should be normalized)
    vec3 lightColor; // Color of the light
    vec3 ambientColor; // Ambient light color
} constants;

void main() {
    // Surface normal comes from the hit attributes
    vec3 normal = attribs;
    
    // Calculate diffuse lighting - dot product of normal and light direction
    float diffuse = max(dot(normal, normalize(constants.lightDir)), 0.0);
    
    // Base material color (could be added as a parameter per sphere)
    vec3 materialColor = vec3(0.8, 0.2, 0.2); // Red material
    
    // Combine ambient and diffuse components
    vec3 ambient = constants.ambientColor * materialColor;
    vec3 diffuseColor = constants.lightColor * materialColor * diffuse;
    
    // Final color is the sum of ambient and diffuse
    payload = ambient + diffuseColor;
}