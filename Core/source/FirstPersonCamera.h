#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

class FirstPersonCamera 
{
private:
    glm::vec3 position;
    float pitch;  // Vertical angle (look up/down)
    float yaw;    // Horizontal angle (look left/right)
    
    float fov;
    float aspectRatio;
    float nearClip;
    float farClip;
    
    float moveSpeed;
    float lookSensitivity;
    
    // Cached matrices
    glm::mat4 viewMatrix;
    glm::mat4 projectionMatrix;
    glm::mat4 inverseViewMatrix;
    glm::mat4 inverseProjectionMatrix;
    
    bool matricesDirty;
    
    void updateMatricesIfNeeded() 
    {
        if (matricesDirty) 
        {
            updateMatrices();
            matricesDirty = false;
        }
    }
    
    void updateMatrices() 
    {
        glm::vec3 direction;
        direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        direction.y = sin(glm::radians(pitch));
        direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        
        glm::vec3 front = glm::normalize(direction);
        glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
        glm::vec3 up = glm::normalize(glm::cross(right, front));
        
        viewMatrix = glm::lookAt(position, position + front, up);
        inverseViewMatrix = glm::inverse(viewMatrix);
        
        projectionMatrix = glm::perspective(glm::radians(fov), aspectRatio, nearClip, farClip);
        inverseProjectionMatrix = glm::inverse(projectionMatrix);
    }
    
public:
    FirstPersonCamera(
        const glm::vec3& pos = glm::vec3(0.0f, 0.0f, 3.0f),
        float fovy = 45.0f,
        float aspect = 16.0f/9.0f,
        float near = 0.1f,
        float far = 1000.0f
    ) : position(pos),
        pitch(0.0f),
        yaw(-90.0f),          
        fov(fovy),
        aspectRatio(aspect),
        nearClip(near),
        farClip(far),
        moveSpeed(5.0f),
        lookSensitivity(0.1f),
        matricesDirty(true) 
    {
        updateMatrices();
    }
    
    // Movement methods
    void moveForward(float deltaTime) 
    {
        glm::vec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        position += glm::normalize(front) * moveSpeed * deltaTime;
        matricesDirty = true;
    }
    
    void moveBackward(float deltaTime) 
    {
        glm::vec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        position -= glm::normalize(front) * moveSpeed * deltaTime;
        matricesDirty = true;
    }
    
    void moveLeft(float deltaTime) 
    {
        glm::vec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
        position -= right * moveSpeed * deltaTime;
        matricesDirty = true;
    }
    
    void moveRight(float deltaTime) 
    {
        glm::vec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
        position += right * moveSpeed * deltaTime;
        matricesDirty = true;
    }
    
    void moveUp(float deltaTime) 
    {
        position.y -= moveSpeed * deltaTime;
        matricesDirty = true;
    }
    
    void moveDown(float deltaTime) 
    {
        position.y += moveSpeed * deltaTime;
        matricesDirty = true;
    }

    void look(float xOffset, float yOffset) 
    {
        xOffset *= lookSensitivity;
        yOffset *= lookSensitivity;
        
        yaw += xOffset;
        pitch += yOffset;
        
        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;
        
        matricesDirty = true;
    }
    
    void setPosition(const glm::vec3& pos) 
    {
        position = pos;
        matricesDirty = true;
    }
    
    void setLookDirection(float pitchValue, float yawValue) 
    {
        pitch = pitchValue;
        yaw = yawValue;
        
        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;
        
        matricesDirty = true;
    }
    
    void setFov(float fovy) 
    {
        fov = fovy;
        matricesDirty = true;
    }
    
    void setAspectRatio(float aspect) 
    {
        aspectRatio = aspect;
        matricesDirty = true;
    }
    
    void setClipPlanes(float near, float far) 
    {
        nearClip = near;
        farClip = far;
        matricesDirty = true;
    }
    
    void setMoveSpeed(float speed) 
    {
        moveSpeed = speed;
    }
    
    void setLookSensitivity(float sensitivity) 
    {
        lookSensitivity = sensitivity;
    }
    
    glm::vec3 getPosition() const 
    {
        return position;
    }
    
    glm::vec3 getForwardDirection() const 
    {
        glm::vec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        return glm::normalize(front);
    }
    
    glm::vec3 getRightDirection() const 
    {
        return glm::normalize(glm::cross(getForwardDirection(), glm::vec3(0.0f, 1.0f, 0.0f)));
    }
    
    glm::vec3 getUpDirection() const 
    {
        return glm::normalize(glm::cross(getRightDirection(), getForwardDirection()));
    }
    
    float getFov() const 
    {
        return fov;
    }
    
    float getAspectRatio() const 
    {
        return aspectRatio;
    }
    
    const glm::mat4& getViewMatrix() 
    {
        updateMatricesIfNeeded();
        return viewMatrix;
    }
    
    const glm::mat4& getProjectionMatrix() 
    {
        updateMatricesIfNeeded();
        return projectionMatrix;
    }
    
    const glm::mat4& getInverseViewMatrix() 
    {
        updateMatricesIfNeeded();
        return inverseViewMatrix;
    }
    
    const glm::mat4& getInverseProjectionMatrix() 
    {
        updateMatricesIfNeeded();
        return inverseProjectionMatrix;
    }
    
    glm::mat4 getViewProjectionMatrix() 
    {
        updateMatricesIfNeeded();
        return projectionMatrix * viewMatrix;
    }
    
    glm::mat4 getInverseViewProjectionMatrix() 
    {
        updateMatricesIfNeeded();
        return inverseViewMatrix * inverseProjectionMatrix;
    }
};
