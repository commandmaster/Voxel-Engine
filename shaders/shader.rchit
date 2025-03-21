#version 460
#extension GL_EXT_ray_tracing : require

hitAttributeEXT vec3 hitNormal;

layout(location = 0) rayPayloadEXT vec3 hitValue;

void main() {
    vec3 lightDir = normalize(vec3(1,1,1));
    float diff = max(dot(hitNormal, lightDir), 0.0);
    vec3 color = vec3(1,0,0) * (diff + 0.1);
	// hitValue = color;

    hitValue = vec3(1,0,0);
    
}