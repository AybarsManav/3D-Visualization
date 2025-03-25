#include "texture.h"

namespace volume {

Texture::Texture(const Texture& other)
    : m_dims(other.getDims())
    , m_texId(other.getTexId())
{
}

// Constructor for float textures
Texture::Texture(const std::vector<float>& floatTexture, glm::ivec3 dims)
    : m_dims(dims)
{
    glGenTextures(1, &m_texId);
    if (dims[2] == 0) {
        glBindTexture(GL_TEXTURE_2D, m_texId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, m_dims.x, m_dims.y, 0, GL_RED, GL_FLOAT, floatTexture.data());
        glBindTexture(GL_TEXTURE_2D, 0); // Unbind the texture
    } else {
        glBindTexture(GL_TEXTURE_3D, m_texId);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, m_dims.x, m_dims.y, m_dims.z, 0, GL_RED, GL_FLOAT, floatTexture.data());
        glBindTexture(GL_TEXTURE_3D, 0); // Unbind the texture
    }
}

// Constructor for vec3 textures
Texture::Texture(const std::vector<glm::vec3>& vec3Texture, glm::ivec3 dims)
    : m_dims(dims)
{
    glGenTextures(1, &m_texId);
    if (dims[2] == 0) {
        glBindTexture(GL_TEXTURE_2D, m_texId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, m_dims.x, m_dims.y, 0, GL_RGB, GL_FLOAT, vec3Texture.data());
        glBindTexture(GL_TEXTURE_2D, 0); // Unbind the texture
    } else {
        glBindTexture(GL_TEXTURE_3D, m_texId);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB32F, m_dims.x, m_dims.y, m_dims.z, 0, GL_RGB, GL_FLOAT, vec3Texture.data());
        glBindTexture(GL_TEXTURE_3D, 0); // Unbind the texture
    }
}

// Constructor for vec4 textures
Texture::Texture(const std::vector<glm::vec4>& vec4Texture, glm::ivec3 dims)
    : m_dims(dims)
{
    glGenTextures(1, &m_texId);
    if (dims[2] == 0) {
        glBindTexture(GL_TEXTURE_2D, m_texId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, m_dims.x, m_dims.y, 0, GL_RGBA, GL_FLOAT, vec4Texture.data());
        glBindTexture(GL_TEXTURE_2D, 0); // Unbind the texture
    } else {
        glBindTexture(GL_TEXTURE_3D, m_texId);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA32F, m_dims.x, m_dims.y, m_dims.z, 0, GL_RGBA, GL_FLOAT, vec4Texture.data());
        glBindTexture(GL_TEXTURE_3D, 0); // Unbind the texture
    }
}

// Destructor
Texture::~Texture()
{
    //Commented it out since the program was deleting textures for some reason (this is bad practice as there is no clean up now)
    //glDeleteTextures(1, &textureID);
}

Texture& Texture::operator=(const Texture& other)
{
    if (this == &other)
        return *this; // Handle self-assignment

    // Clean up existing textures
    glDeleteTextures(1, &m_texId);

    m_texId = other.getTexId();
    m_dims = other.m_dims;

    return *this;
}

GLuint Texture::getTexId() const
{
    return m_texId;
}

GLuint* Texture::getTexIdPointer()
{
    return &m_texId;
}

glm::ivec3 Texture::getDims() const
{
    return m_dims;
}

void Texture::setInterpolationMode(GLint interpolationMode)
{
    if (m_dims[2] == 0) {
        glBindTexture(GL_TEXTURE_2D, m_texId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, interpolationMode);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, interpolationMode);
        glBindTexture(GL_TEXTURE_2D, 0); // Unbind the texture
    } else {
        glBindTexture(GL_TEXTURE_3D, m_texId);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, interpolationMode);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, interpolationMode);
        glBindTexture(GL_TEXTURE_3D, 0); // Unbind the texture
    } 
}

void Texture::update(const std::vector<float>& floatTexture, glm::ivec3 dims)
{
    m_dims = dims;
    if (dims[2] == 0) {
        glBindTexture(GL_TEXTURE_2D, m_texId);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, m_dims.x, m_dims.y, 0, GL_RED, GL_FLOAT, floatTexture.data());
        glBindTexture(GL_TEXTURE_2D, 0); // Unbind the texture
    } else {
        glBindTexture(GL_TEXTURE_3D, m_texId);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, m_dims.x, m_dims.y, m_dims.z, 0, GL_RED, GL_FLOAT, floatTexture.data());
        glBindTexture(GL_TEXTURE_3D, 0); // Unbind the texture
    }
}

void Texture::update(const std::vector<glm::vec3>& vec3Texture, glm::ivec3 dims)
{
    m_dims = dims;
    if (dims[2] == 0) {
        glBindTexture(GL_TEXTURE_2D, m_texId);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, m_dims.x, m_dims.y, 0, GL_RGB, GL_FLOAT, vec3Texture.data());
        glBindTexture(GL_TEXTURE_2D, 0); // Unbind the texture
    } else {
        glBindTexture(GL_TEXTURE_3D, m_texId);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB32F, m_dims.x, m_dims.y, m_dims.z, 0, GL_RGB, GL_FLOAT, vec3Texture.data());
        glBindTexture(GL_TEXTURE_3D, 0); // Unbind the texture
    }
}

void Texture::update(const std::vector<glm::vec4>& vec4Texture, glm::ivec3 dims)
{
    m_dims = dims;
    if (dims[2] == 0) {
        glBindTexture(GL_TEXTURE_2D, m_texId);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, m_dims.x, m_dims.y, 0, GL_RGBA, GL_FLOAT, vec4Texture.data());
        glBindTexture(GL_TEXTURE_2D, 0); // Unbind the texture
    } else {
        glBindTexture(GL_TEXTURE_3D, m_texId);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA32F, m_dims.x, m_dims.y, m_dims.z, 0, GL_RGBA, GL_FLOAT, vec4Texture.data());
        glBindTexture(GL_TEXTURE_3D, 0); // Unbind the texture
    }
}
}