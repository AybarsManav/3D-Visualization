#include "texture_manager.h"

namespace volume {

// Constructor
TextureManager::TextureManager()
    : textureList(std::vector<Texture>())
{
}

// Copy Constructor
TextureManager::TextureManager(const TextureManager& other)
    : textureList(other.textureList)
{
    // Ensure each texture is correctly copied
    for (Texture& texture : textureList) {
        GLuint texId;
        glGenTextures(1, texture.getTexIdPointer());
        glBindTexture(GL_TEXTURE_2D, texture.getTexId());
        glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 0, 0, texture.getDims().x, texture.getDims().y, 0);
    }
}

// Copy Assignment Operator 
TextureManager& TextureManager::operator=(const TextureManager& other)
{
    if (this == &other)
        return *this; // Handle self-assignment

    // Clean up existing textures
    for (Texture& texture : textureList) {
        glDeleteTextures(1, texture.getTexIdPointer());
    }

    // Copy textures from other
    textureList = other.textureList;
    for (Texture& texture : textureList) {
        glGenTextures(1, texture.getTexIdPointer());
        glBindTexture(GL_TEXTURE_2D, texture.getTexId());
        glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 0, 0, texture.getDims().x, texture.getDims().y, 0);
    }

    return *this;
}

TextureManager::~TextureManager()
{
    for (Texture& texture : textureList) {
        glDeleteTextures(1, texture.getTexIdPointer());
    }
}

int TextureManager::addTexture(const std::vector<float> floatTexture, glm::ivec3 dims)
{
    textureList.push_back(Texture(floatTexture, dims));
    return textureList.size() - 1;
}

int TextureManager::addTexture(const std::vector<glm::vec4> vec4Texture, glm::ivec3 dims)
{
    textureList.push_back(Texture(vec4Texture, dims));
    return textureList.size() - 1;
}

Texture TextureManager::getTexture(int index)
{
    return textureList[index];
}

void TextureManager::setInterpolationModeNN()
{
    for (Texture& texture : textureList) {
        texture.setInterpolationMode(GL_NEAREST);
    }
}

void TextureManager::setInterpolationModeLinear()
{
    for (Texture& texture : textureList) {
        texture.setInterpolationMode(GL_LINEAR);
    }
}

}
