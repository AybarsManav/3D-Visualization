#pragma once

#ifdef __linux__
#include <GL/glew.h>
#else
#include <gl/glew.h>
#endif
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <vector>

namespace volume {

class Texture {
public:
    // Copy constructor needed for the textureManager
    Texture(const Texture& other);
    Texture(const std::vector<float>& floatTexture, glm::ivec3 dims);
    Texture(const std::vector<glm::vec3>& vec3Texture, glm::ivec3 dims);
    Texture(const std::vector<glm::vec4>& vec4Texture, glm::ivec3 dims);

    ~Texture();

    Texture& operator=(const Texture& other);

    GLuint getTexId() const;
    GLuint* getTexIdPointer();

    glm::ivec3 getDims() const;

    void setInterpolationMode(GLint interpolationMode);

    void update(const std::vector<float>& floatTexture, glm::ivec3 dims);
    void update(const std::vector<glm::vec3>& vec3Texture, glm::ivec3 dims);
    void update(const std::vector<glm::vec4>& vec4Texture, glm::ivec3 dims);

private:
    glm::ivec3 m_dims; // dimensions of the texture
    GLuint m_texId; 
};
}