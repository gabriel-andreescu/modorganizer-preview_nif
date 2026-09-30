#pragma once

#include "Nif/Fo4Material.h"
#include "Render/ShaderManager.h"
#include "Render/ShapeMaterial.h"
#include "Textures/SlotDescriptors.h"
#include "Textures/Slots.h"

#include <QString>

#include <array>
#include <cstddef>
#include <vector>

class PreviewTexture;
class QOpenGLShaderProgram;
class TextureManager;

namespace nifly {
class BSEffectShaderProperty;
class NifFile;
class NiShader;
}

struct TextureSlotState {
    TextureSlotDescriptor descriptor;
    QString requestedPath;
    bool loaded = false;
    TextureFallback fallback = TextureFallback::kNone;
};

class OpenGLShapeTextures {
public:
    void InitializeDescriptors(
        const nifly::NiShader* a_shader,
        ShaderManager::ShaderType a_shaderType,
        bool a_isRefractionProxy
    );
    void Load(
        nifly::NifFile* a_nifFile,
        nifly::NiShader* a_shader,
        ShaderManager::ShaderType a_shaderType,
        bool a_isPBR,
        const Fo4Material::Material& a_material,
        TextureManager* a_textureManager
    );
    void UseDefaultTextures(TextureManager* a_textureManager);
    void SetupUniforms(QOpenGLShaderProgram* a_program, const OpenGLShapeTextureFeatureFlags& a_materialFlags) const;

    [[nodiscard]] std::vector<TextureSlotState> SlotStates() const;

private:
    void LoadEffectShaderTextures(
        nifly::BSEffectShaderProperty* a_shader,
        ShaderManager::ShaderType a_shaderType,
        TextureManager* a_textureManager
    );
    void LoadFo4MaterialTextures(const Fo4Material::Textures& a_textures, TextureManager* a_textureManager);
    void LoadFo4EffectMaterialTextures(const Fo4Material::Textures& a_textures, TextureManager* a_textureManager);
    void LoadTextureSetTextures(nifly::NifFile* a_nifFile, nifly::NiShader* a_shader, TextureManager* a_textureManager);
    void AssignFallback(TextureManager* a_textureManager, std::size_t a_textureSlot, TextureFallback a_fallback);
    void EnsurePBRTextureDefaults(TextureManager* a_textureManager);
    void BindTextures() const;

    TextureSlotDescriptorList _slotDescriptors {};
    std::array<PreviewTexture*, kTextureSlotCount> _textures {nullptr};
    std::array<bool, kTextureSlotCount> _loadedTextures {};
    std::array<QString, kTextureSlotCount> _requestedPaths {};
    std::array<TextureFallback, kTextureSlotCount> _appliedFallbacks {};
    bool _hasSourceTexture = false;
    bool _hasGreyscaleMap = false;
};
