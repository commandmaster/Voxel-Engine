#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera 
{
public:
    enum class ProjectionType { Perspective, Orthographic };

    Camera() = default;
    
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
        viewProjMatrix = projMatrix * viewMatrix;
    }

    void updateView() 
    {
        viewMatrix = glm::mat4_cast(orientation) * glm::translate(glm::mat4(1.0f), -position);
        viewProjMatrix = projMatrix * viewMatrix;
        invViewMatrix = glm::inverse(viewMatrix);
        invProjMatrix = glm::inverse(projMatrix);
    }

    void lookAt(const glm::vec3& target, const glm::vec3& up = {0, 1, 0}) 
    {
        wasUpdated = true;
        viewMatrix = glm::lookAt(position, target, up);
        invViewMatrix = glm::inverse(viewMatrix);
        orientation = glm::quat_cast(viewMatrix);
    }

    // Movement functions
    void move(const glm::vec3& delta) 
    {
        wasUpdated = true;
        position += delta;
        updateView();
    }

    void rotate(float rawYaw, float rawPitch) 
    {
        float yaw = rawYaw * mouseSensitivity;
        float pitch = rawPitch * mouseSensitivity;

        wasUpdated = true;
        const float maxPitch = glm::radians(89.0f);

        currentYaw += yaw;
        currentPitch = glm::clamp(currentPitch + pitch, -maxPitch, maxPitch);
        
        orientation = glm::quat(glm::vec3(currentPitch, currentYaw, 0.0f));
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
    glm::vec3 getForward() const { return invViewMatrix[2]; }
    glm::vec3 getRight() const { return invViewMatrix[0]; }
    glm::vec3 getUp() const { return invViewMatrix[1]; }
    bool getWasUpdated() const { return wasUpdated; }
    float getMouseSensitivity() const { return mouseSensitivity; }

    // Setters
    void setPosition(const glm::vec3& pos) { position = pos; updateView(); }
    void setOrientation(const glm::quat& rot) { orientation = rot; updateView(); }
    void setWasUpdated(const bool flag) { wasUpdated = flag; }
    void setMouseSensitivity(float sensitivity) { mouseSensitivity = sensitivity; }

private:
    // Flags
    bool wasUpdated = true; // inital update required

    float currentYaw = 0.0f;
    float currentPitch = 0.0f;
    float mouseSensitivity = 0.1f;

    glm::vec3 position{0.0f};
    glm::quat orientation{1.0f, 0.0f, 0.0f, 0.0f};
    
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