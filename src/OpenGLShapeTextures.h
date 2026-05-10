#pragma once

#include "OpenGLShapeMaterial.h"
#include "ShaderManager.h"
#include "TextureSlotDescriptors.h"
#include "TextureSlots.h"

#include <array>
#include <cstddef>

class PreviewTexture;
class QOpenGLShaderProgram;
class TextureManager;

namespace nifly {
class BSEffectShaderProperty;
class NifFile;
class NiShader;
}

class OpenGLShapeTextures {
public:
    void initializeDescriptors(
        const nifly::NiShader* shader,
        ShaderManager::ShaderType shaderType,
        bool isRefractionProxy
    );
    void load(
        nifly::NifFile* nifFile,
        nifly::NiShader* shader,
        ShaderManager::ShaderType shaderType,
        bool isPBR,
        TextureManager* textureManager
    );
    void useDefaultTextures(TextureManager* textureManager);
    void setupUniforms(QOpenGLShaderProgram* program, const OpenGLShapeTextureFeatureFlags& materialFlags) const;

private:
    void loadEffectShaderTextures(
        nifly::BSEffectShaderProperty* shader,
        ShaderManager::ShaderType shaderType,
        TextureManager* textureManager
    );
    bool loadFo4MaterialTextures(nifly::NiShader* shader, TextureManager* textureManager);
    bool loadFo4EffectMaterialTextures(nifly::NiShader* shader, TextureManager* textureManager);
    void loadTextureSetTextures(nifly::NifFile* nifFile, nifly::NiShader* shader, TextureManager* textureManager);
    void assignMissingTexture(TextureManager* textureManager, std::size_t textureSlot);
    void ensurePBRTextureDefaults(TextureManager* textureManager);
    void bindTextures() const;

    TextureSlotDescriptorList m_SlotDescriptors {};
    std::array<PreviewTexture*, TextureSlotCount> m_Textures {nullptr};
    std::array<bool, TextureSlotCount> m_LoadedTextures {};
    bool m_HasSourceTexture = false;
    bool m_HasGreyscaleMap = false;
};
