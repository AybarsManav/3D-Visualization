#include "texture.h"
#include <vector>

namespace volume {

// It turned out we didn't need to use this class so it remains untested, use with caution 
class TextureManager {
public:
    TextureManager();
    TextureManager(const TextureManager& other);
    TextureManager& operator=(const TextureManager& other);

    ~TextureManager();

    int addTexture(const std::vector<float> floatTexture, glm::ivec3 dims);
    int addTexture(const std::vector<glm::vec4> vec4Texture, glm::ivec3 dims);

    Texture getTexture(int index);

    void setInterpolationModeNN();
    void setInterpolationModeLinear();

protected:
    std::vector<Texture> textureList;
};

}