#pragma once

#include <QOpenGLShaderProgram>
#include <uibase/imoinfo.h>

#include <array>

enum VertexAttrib {
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
    enum ShaderType {
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

    explicit ShaderManager(MOBase::IOrganizer* a_moInfo);
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

    MOBase::IOrganizer* _moInfo;
    std::array<QOpenGLShaderProgram*, kShaderCount> _programs {};
};
