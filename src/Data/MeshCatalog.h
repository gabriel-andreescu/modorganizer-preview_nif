#pragma once

#include "Data/LoadOrder.h"

#include <QObject>
#include <QSet>
#include <QStringList>
#include <QVector>

class ArchiveIndex;

namespace MOBase {
class IOrganizer;
}

// Finds every mesh path: the loose meshes MO2 knows, then the meshes in the loaded archives, and
// reports them as it finds them. The archive library is shared with MO2's plugins and isn't built for calls from two
// threads at once, so archives are read on the UI thread, a short slice per event-loop pass.
class MeshCatalog final : public QObject {
    Q_OBJECT

public:
    MeshCatalog(const MOBase::IOrganizer& a_organizer, ArchiveIndex& a_archiveIndex, QObject* a_parent = nullptr);

    // Finds the paths again from the files as MO2 sees them now.
    void Refresh();
    [[nodiscard]] bool IsReady() const noexcept {
        return _ready;
    }

signals:
    // Data paths with forward slashes. A refresh reports each mesh once, without regard to case.
    void Found(const QStringList& a_dataPaths);
    void Progress(int a_archivesRead, int a_archives);
    void Ready();

private:
    void SchedulePass();
    void RunPass();
    void FindMore();
    void FindLooseMeshes();
    void Add(const QString& a_dataPath);
    void ReportBatch();

    const MOBase::IOrganizer* _organizer;
    ArchiveIndex* _archiveIndex;
    QVector<LoadedArchive> _archives;
    qsizetype _nextArchive = 0;
    bool _looseMeshesFound = false;
    // Lowercase paths already found, so a mesh several sources supply is listed once.
    QSet<QString> _seen;
    // The meshes found by the last pass that found any, and how many of them were reported.
    QStringList _unreported;
    qsizetype _reported = 0;
    // Bumped by each refresh, so a pass queued by an earlier refresh stops.
    int _generation = 0;
    bool _ready = false;
};
