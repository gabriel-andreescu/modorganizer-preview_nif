#pragma once

#include <NifFile.hpp>

#include <QByteArray>
#include <QString>
#include <QVector>

#include <memory>

namespace MOBase
{
class IOrganizer;
}

enum class NifPreviewProviderKind
{
  LooseFile,
  Archive,
  InMemory
};

struct NifPreviewProvider
{
  QString displayName;
  QString virtualPath;
  NifPreviewProviderKind kind = NifPreviewProviderKind::LooseFile;
  QString absolutePath;
  QString archivePath;
  QString archiveName;
  QByteArray data;
};

struct NifPreviewSourceSet
{
  QVector<NifPreviewProvider> providers;
  QString virtualPath;
  int currentIndex = 0;
};

class NifPreviewSourceResolver
{
public:
  static NifPreviewSourceSet resolve(MOBase::IOrganizer* organizer,
                                     const QString& fileName,
                                     const QByteArray& fileData);
};

std::shared_ptr<nifly::NifFile> loadNifProvider(const NifPreviewProvider& provider);
QString makeNifStatsText(const nifly::NifFile* nifFile);
