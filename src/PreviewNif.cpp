#include <NifFile.hpp>

#include "NifExtensions.h"
#include "NifWidget.h"
#include "PreviewNif.h"

#include <QGridLayout>
#include <filesystem>
#include <sstream>

bool PreviewNif::init(MOBase::IOrganizer* moInfo)
{
  m_MOInfo = moInfo;
  return true;
}

QString PreviewNif::name() const
{
  return "Preview NIF";
}

QString PreviewNif::author() const
{
  return "Parapets";
}

QString PreviewNif::description() const
{
  return "Supports previewing NIF files";
}

MOBase::VersionInfo PreviewNif::version() const
{
  return {0, 3, 0, 0, MOBase::VersionInfo::RELEASE_BETA};
}

QList<MOBase::PluginSetting> PreviewNif::settings() const
{
  return {};
}

bool PreviewNif::enabledByDefault() const
{
  return true;
}

std::set<QString> PreviewNif::supportedExtensions() const
{
  return {"bto", "btr", "nif"};
}

bool PreviewNif::supportsArchives() const
{
  return true;
}

QWidget* PreviewNif::genFilePreview(const QString& fileName, const QSize& maxSize) const
{
  return genDataPreview(nullptr, fileName, maxSize);
}

QWidget* PreviewNif::genDataPreview(const QByteArray& fileData, const QString& fileName,
                                    const QSize& maxSize) const
{
  auto path = std::filesystem::path(fileName.toStdWString());
  std::shared_ptr<nifly::NifFile> nifFile;

  if (fileData != nullptr && !fileData.isEmpty()) {
    const auto fileStream =
        std::make_shared<std::istringstream>(fileData.toStdString());
    nifFile = std::make_shared<nifly::NifFile>(*fileStream);
  } else {
    nifFile = std::make_shared<nifly::NifFile>(path);
  }

  if (!nifFile->IsValid()) {
    qWarning(qUtf8Printable(tr("Failed to load file: %1").arg(fileName)));
    return nullptr;
  }

  const auto layout = new QGridLayout();
  layout->setRowStretch(0, 1);
  layout->setColumnStretch(0, 1);

  layout->addWidget(makeLabel(nifFile.get()), 1, 0, 1, 1);

  const auto nifWidget = new NifWidget(nifFile, m_MOInfo);
  layout->addWidget(nifWidget, 0, 0, 1, 1);

  const auto widget = new QWidget();
  widget->setLayout(layout);
  return widget;
}

QLabel* PreviewNif::makeLabel(const nifly::NifFile* nifFile)
{
  unsigned int shapes = 0;
  unsigned int faces  = 0;
  unsigned int verts  = 0;

  for (const auto& shape : nifFile->GetShapes()) {
    shapes++;
    faces += shape->GetNumTriangles();
    verts += shape->GetNumVertices();
  }

  const auto text =
      tr("Verts: %1 | Faces: %2 | Shapes: %3").arg(verts).arg(faces).arg(shapes);
  const auto label = new QLabel(text);
  label->setWordWrap(true);
  label->setTextInteractionFlags(Qt::TextSelectableByMouse);
  return label;
}
