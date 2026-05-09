#pragma once

#include <QSharedPointer>
#include <QWeakPointer>
#include <uibase/ipluginpreview.h>

class Camera;

class PreviewNif final : public MOBase::IPluginPreview
{
  Q_OBJECT
  Q_INTERFACES(MOBase::IPlugin MOBase::IPluginPreview)
  Q_PLUGIN_METADATA(IID "org.tannin.PreviewNif" FILE "previewnif.json")

public:
  PreviewNif() = default;

  // IPlugin Interface

  bool init(MOBase::IOrganizer* moInfo) override;
  [[nodiscard]] QString name() const override;
  [[nodiscard]] QString author() const override;
  [[nodiscard]] QString description() const override;
  [[nodiscard]] MOBase::VersionInfo version() const override;
  [[nodiscard]] QList<MOBase::PluginSetting> settings() const override;
  [[nodiscard]] bool enabledByDefault() const override;

  // IPluginPreview interface

  [[nodiscard]] std::set<QString> supportedExtensions() const override;
  [[nodiscard]] bool supportsArchives() const override;
  [[nodiscard]] QWidget* genFilePreview(const QString& fileName,
                                        const QSize& maxSize) const override;
  [[nodiscard]] QWidget* genDataPreview(const QByteArray& fileData,
                                        const QString& fileName,
                                        const QSize& maxSize) const override;

private:
  [[nodiscard]] QSharedPointer<Camera> sharedCamera() const;

  MOBase::IOrganizer* m_MOInfo{};
  mutable QWeakPointer<Camera> m_SharedCamera;
};
