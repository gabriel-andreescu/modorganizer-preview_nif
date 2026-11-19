#include "Data/MeshCatalog.h"
#include "Data/ArchiveIndex.h"
#include "Data/MeshFiles.h"

#include <QElapsedTimer>
#include <QTimer>

#include <uibase/ifiletree.h>
#include <uibase/imoinfo.h>

#include <functional>

namespace {
// How long one pass reads archives before it lets MO2 handle input and repaint.
constexpr qint64 kPassBudgetMs = 40;
// The most meshes one pass reports. A single archive can hold over a hundred thousand, and a list takes that many
// at once with a visible stall.
constexpr qsizetype kBatchSize = 20000;

void CollectMeshes(
    const MOBase::IFileTree& a_directory,
    const QString& a_prefix,
    const std::function<void(const QString&)>& a_add
) {
    for (const auto& entry : a_directory) {
        const QString path = a_prefix + entry->name();
        if (const auto subdirectory = entry->astree()) {
            CollectMeshes(*subdirectory, path + '/', a_add);
        } else if (MeshFiles::IsMeshPath(path)) {
            a_add(path);
        }
    }
}
} // namespace

MeshCatalog::MeshCatalog(const MOBase::IOrganizer& a_organizer, ArchiveIndex& a_archiveIndex, QObject* a_parent)
    : QObject(a_parent)
    , _organizer(&a_organizer)
    , _archiveIndex(&a_archiveIndex) {}

void MeshCatalog::Refresh() {
    ++_generation;
    _ready = false;
    _seen.clear();
    _unreported.clear();
    _reported = 0;
    _archives = LoadOrder::LoadedArchives(*_organizer);
    _nextArchive = 0;
    _looseMeshesFound = false;

    emit Progress(0, static_cast<int>(_archives.size()));
    SchedulePass();
}

void MeshCatalog::SchedulePass() {
    QTimer::singleShot(0, this, [this, generation = _generation] {
        if (generation == _generation) {
            RunPass();
        }
    });
}

void MeshCatalog::RunPass() {
    if (_reported == _unreported.size()) {
        _unreported.clear();
        _reported = 0;
        FindMore();
    }
    ReportBatch();
    emit Progress(static_cast<int>(_nextArchive), static_cast<int>(_archives.size()));

    if (_reported < _unreported.size() || _nextArchive < _archives.size()) {
        SchedulePass();
        return;
    }
    _seen.clear();
    _unreported.clear();
    _reported = 0;
    _ready = true;
    emit Ready();
}

void MeshCatalog::FindMore() {
    if (!_looseMeshesFound) {
        _looseMeshesFound = true;
        FindLooseMeshes();
        return;
    }

    QElapsedTimer pass;
    pass.start();
    while (_nextArchive < _archives.size() && pass.elapsed() < kPassBudgetMs) {
        for (const auto& path : _archiveIndex->Members(_archives[_nextArchive].path)->MeshPaths()) {
            Add(path);
        }
        ++_nextArchive;
    }
}

void MeshCatalog::FindLooseMeshes() {
    const auto root = _organizer->virtualFileTree();
    const auto meshes = root ? root->findDirectory(QStringLiteral("meshes")) : nullptr;
    if (meshes) {
        CollectMeshes(*meshes, QStringLiteral("meshes/"), [this](const QString& a_path) { Add(a_path); });
    }
}

void MeshCatalog::Add(const QString& a_dataPath) {
    const auto key = a_dataPath.toLower();
    if (!_seen.contains(key)) {
        _seen.insert(key);
        _unreported.push_back(a_dataPath);
    }
}

void MeshCatalog::ReportBatch() {
    const auto batch = _unreported.mid(_reported, kBatchSize);
    _reported += batch.size();
    if (!batch.isEmpty()) {
        emit Found(batch);
    }
}
