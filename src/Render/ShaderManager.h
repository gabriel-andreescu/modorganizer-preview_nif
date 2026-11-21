#pragma once

#include <QOpenGLShaderProgram>

#include <array>
#include <cstdint>

enum VertexAttrib : std::uint8_t {
    kAttribPosition = 0,
    kAttribNormal,
    kAttribTangent,
    kAttribBitangent,
    kAttribTexCoord,
    kAttribColor,

    kAttribCount,
};

class ShaderManager {
public:
    enum ShaderType : std::int8_t {
        kNone = -1,
        kSKDefault,
        kSKMSN,
        kSKMultilayer,
        kSKEffectShader,
        kSKPBR,
        kSKRefractionProxy,
        kCollisionWire,
        kFO4Default,
        kFO4EffectShader,

        kShaderCount,
    };

    ShaderManager() = default;
    ~ShaderManager() = default;
    ShaderManager(const ShaderManager&) = delete;
    ShaderManager(ShaderManager&&) = delete;
    ShaderManager& operator=(const ShaderManager&) = delete;
    ShaderManager& operator=(ShaderManager&&) = delete;

    QOpenGLShaderProgram* GetProgram(ShaderType a_type);
    // Null until the first draw that uses this shader type.
    [[nodiscard]] const QOpenGLShaderProgram* LoadedProgram(ShaderType a_type) const;

private:
    static QOpenGLShaderProgram* LoadProgram(ShaderType a_type);

    std::array<QOpenGLShaderProgram*, kShaderCount> _programs {};
};
