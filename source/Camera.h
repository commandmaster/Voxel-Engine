#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera 
{
public:
    enum class ProjectionType { Perspective, Orthographic };

    Camera() 
    {
        updateView();
        updateProjection();
    }
    
    void setPerspective(float fovDegrees, float aspectRatio, float nearClip = 0.1f, float farClip = 1000.0f) 
    {
        wasUpdated = true;
        projectionType = ProjectionType::Perspective;
        fov = glm::radians(fovDegrees);
        aspect = aspectRatio;
        zNear = nearClip;
        zFar = farClip;
        updateProjection();
    }

    void setOrthographic(float size, float aspectRatio, float nearClip = 0.1f, float farClip = 1000.0f) 
    {
        wasUpdated = true;
        projectionType = ProjectionType::Orthographic;
        orthoSize = size;
        aspect = aspectRatio;
        zNear = nearClip;
        zFar = farClip;
        updateProjection();
    }

    void updateProjection() 
    {
        wasUpdated = true;
        if (projectionType == ProjectionType::Perspective) 
        {
            projMatrix = glm::perspective(fov, aspect, zNear, zFar);
        } 
        else 
        {
            float width = orthoSize * aspect;
            float height = orthoSize;
            projMatrix = glm::ortho(-width/2, width/2, -height/2, height/2, zNear, zFar);
        }
        projMatrix[1][1] *= -1; // Flip Y-axis for Vulkan
        invProjMatrix = glm::inverse(projMatrix);
        updateViewProj();
    }

    void updateView() 
    {
        wasUpdated = true;
        
        // Create view matrix from position and orientation quaternion
        glm::mat4 rotMatrix = glm::mat4_cast(orientation);
        viewMatrix = glm::translate(rotMatrix, -position);
        
        // Cache the inverse view matrix
        invViewMatrix = glm::inverse(viewMatrix);
        
        updateViewProj();
    }
    
    void updateViewProj()
    {
        viewProjMatrix = projMatrix * viewMatrix;
    }

    void lookAt(const glm::vec3& target, const glm::vec3& up = {0, 1, 0}) 
    {
        wasUpdated = true;
        
        // Create a lookAt matrix and extract rotation
        glm::mat4 lookAtMatrix = glm::lookAt(position, target, up);
        glm::mat3 rotMatrix = glm::mat3(lookAtMatrix);
        
        // Convert to quaternion
        orientation = glm::quat_cast(rotMatrix);
        
        updateView();
    }

    // Movement functions
    void move(const glm::vec3& delta) 
    {
        wasUpdated = true;
        position += delta;
        updateView();
    }

    void moveLocal(const glm::vec3& delta)
    {
        wasUpdated = true;
        position += getRight() * delta.x + getUp() * delta.y + getForward() * delta.z;
        updateView();
    }

    void rotate(float yawDelta, float pitchDelta) 
    {
        wasUpdated = true;
        
        // Apply yaw around global Y axis
        glm::quat yawQuat = glm::angleAxis(yawDelta * mouseSensitivity, glm::vec3(0.0f, 1.0f, 0.0f));
        
        // Apply pitch around local X axis (right vector)
        glm::vec3 rightVector = getRight();
        glm::quat pitchQuat = glm::angleAxis(pitchDelta * mouseSensitivity, rightVector);
        
        // Combine rotations: first yaw, then pitch
        orientation = pitchQuat * orientation * yawQuat;
        
        // Normalize to prevent drift
        orientation = glm::normalize(orientation);
        
        updateView();
    }
    
    void roll(float angle)
    {
        wasUpdated = true;
        
        // Apply roll around local Z axis (forward vector)
        glm::vec3 forwardVector = getForward();
        glm::quat rollQuat = glm::angleAxis(angle * mouseSensitivity, forwardVector);
        
        // Apply roll rotation
        orientation = rollQuat * orientation;
        
        // Normalize to prevent drift
        orientation = glm::normalize(orientation);
        
        updateView();
    }

    // Getters
    const glm::mat4& getView() const { return viewMatrix; }
    const glm::mat4& getProjection() const { return projMatrix; }
    const glm::mat4& getViewProjection() const { return viewProjMatrix; }
    const glm::mat4& getInvView() const { return invViewMatrix; }
    const glm::mat4& getInvProj() const { return invProjMatrix; }
    const glm::vec3& getPosition() const { return position; }
    const glm::quat& getOrientation() const { return orientation; }
    
    glm::vec3 getForward() const { return orientation * glm::vec3(0.0f, 0.0f, -1.0f); }
	glm::vec3 getRight() const { return orientation * glm::vec3(1.0f, 0.0f, 0.0f); }
	glm::vec3 getUp() const { return orientation * glm::vec3(0.0f, 1.0f, 0.0f); }

    bool getWasUpdated() const { return wasUpdated; }
    float getMouseSensitivity() const { return mouseSensitivity; }

    // Setters
    void setPosition(const glm::vec3& pos) { position = pos; updateView(); }
    void setOrientation(const glm::quat& orient) { orientation = glm::normalize(orient); updateView(); }
    void setWasUpdated(const bool flag) { wasUpdated = flag; }
    void setMouseSensitivity(float sensitivity) { mouseSensitivity = sensitivity; }

private:
    // Flags
    bool wasUpdated = true; // initial update required

    float mouseSensitivity = 0.1f;

    // Position and orientation
    glm::vec3 position{0.0f};
    glm::quat orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);  // Identity quaternion
    
    // Projection parameters
    ProjectionType projectionType = ProjectionType::Perspective;
    float fov = glm::radians(60.0f);
    float aspect = 16.0f/9.0f;
    float zNear = 0.1f;
    float zFar = 1000.0f;
    float orthoSize = 10.0f;

    // Matrices
    glm::mat4 viewMatrix{1.0f};
    glm::mat4 projMatrix{1.0f};
    glm::mat4 viewProjMatrix{1.0f};
    glm::mat4 invViewMatrix{1.0f};
    glm::mat4 invProjMatrix{1.0f};
};