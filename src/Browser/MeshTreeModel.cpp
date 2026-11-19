#include "Browser/MeshTreeModel.h"

#include <QApplication>
#include <QDir>
#include <QStyle>

#include <algorithm>
#include <iterator>
#include <utility>

namespace {
constexpr int kNameColumn = 0;
constexpr int kModColumn = 1;
constexpr int kColumns = 2;
constexpr int kRoot = 0;
} // namespace

MeshTreeModel::MeshTreeModel(QObject* a_parent)
    : QAbstractItemModel(a_parent)
    , _nodes(1)
    , _folderIcon(QApplication::style()->standardIcon(QStyle::SP_DirIcon))
    , _meshIcon(QApplication::style()->standardIcon(QStyle::SP_FileIcon)) {}

void MeshTreeModel::AddMeshes(const QStringList& a_paths) {
    if (a_paths.isEmpty()) {
        return;
    }

    // A batch lands all over the tree, so it is announced as one layout change. Views keep their open folders and
    // their selection through it.
    emit layoutAboutToBeChanged();
    std::vector<int> grown;
    for (const auto& path : a_paths) {
        const auto nameStart = path.lastIndexOf('/') + 1;
        const auto folder = FolderAt(path.left(nameStart), grown);
        Append({.name = path.mid(nameStart), .path = path, .parent = folder}, grown);
    }
    _meshCount += a_paths.size();
    SortChildren(grown);
    MovePersistentIndexes();
    emit layoutChanged();
}

void MeshTreeModel::SetWinnerLabel(WinnerLabel a_winnerLabel) {
    _winnerLabel = std::move(a_winnerLabel);
    _winnerLabels.clear();

    for (std::size_t folder = 0; folder < _nodes.size(); ++folder) {
        const auto& node = _nodes[folder];
        if (node.path.isEmpty() && !node.children.empty()) {
            const auto parent = folder == kRoot ? QModelIndex() : createIndex(node.row, kNameColumn, folder);
            const auto lastRow = static_cast<int>(node.children.size()) - 1;
            emit dataChanged(index(0, kModColumn, parent), index(lastRow, kModColumn, parent), {Qt::DisplayRole});
        }
    }
}

int MeshTreeModel::FolderAt(const QString& a_folderPath, std::vector<int>& a_grown) {
    if (a_folderPath.isEmpty()) {
        return kRoot;
    }

    const auto key = a_folderPath.toLower();
    if (const auto found = _folders.constFind(key); found != _folders.cend()) {
        return *found;
    }

    const auto nameStart = a_folderPath.lastIndexOf('/', -2) + 1;
    const auto parent = FolderAt(a_folderPath.left(nameStart), a_grown);
    const auto name = a_folderPath.mid(nameStart, a_folderPath.size() - nameStart - 1);
    return *_folders.insert(key, Append({.name = name, .parent = parent}, a_grown));
}

int MeshTreeModel::Append(Node a_node, std::vector<int>& a_grown) {
    const auto parent = a_node.parent;
    const auto node = static_cast<int>(_nodes.size());
    _nodes.push_back(std::move(a_node));
    _nodes[parent].children.push_back(node);
    a_grown.push_back(parent);
    return node;
}

// Folders come before meshes, each group by name. A folder's earlier children are in order already, so its new ones
// are sorted by themselves and merged in.
void MeshTreeModel::SortChildren(std::vector<int>& a_folders) {
    std::ranges::sort(a_folders);
    const auto repeated = std::ranges::unique(a_folders);
    a_folders.erase(repeated.begin(), repeated.end());

    const auto comesFirst = [this](const int a_lhs, const int a_rhs) {
        const auto& lhs = _nodes[a_lhs];
        const auto& rhs = _nodes[a_rhs];
        if (lhs.path.isEmpty() != rhs.path.isEmpty()) {
            return lhs.path.isEmpty();
        }
        return lhs.name.compare(rhs.name, Qt::CaseInsensitive) < 0;
    };
    for (const auto folder : a_folders) {
        auto& node = _nodes[folder];
        const auto added = std::ranges::next(node.children.begin(), node.sortedChildren);
        std::ranges::sort(added, node.children.end(), comesFirst);
        std::ranges::inplace_merge(node.children, added, comesFirst);
        node.sortedChildren = std::ssize(node.children);
        for (std::size_t row = 0; row < node.children.size(); ++row) {
            _nodes[node.children[row]].row = static_cast<int>(row);
        }
    }
}

void MeshTreeModel::MovePersistentIndexes() {
    const auto from = persistentIndexList();
    QModelIndexList moved;
    moved.reserve(from.size());
    for (const auto& index : from) {
        moved.push_back(createIndex(_nodes[index.internalId()].row, index.column(), index.internalId()));
    }
    changePersistentIndexList(from, moved);
}

const MeshTreeModel::Node& MeshTreeModel::NodeOf(const QModelIndex& a_index) const {
    return _nodes[a_index.isValid() ? static_cast<std::size_t>(a_index.internalId()) : kRoot];
}

QString MeshTreeModel::DataPath(const QModelIndex& a_index) const {
    return a_index.isValid() ? NodeOf(a_index).path : QString();
}

QModelIndex MeshTreeModel::IndexOf(const QString& a_dataPath) const {
    const auto found = std::ranges::find_if(_nodes, [&](const Node& a_node) {
        return !a_node.path.isEmpty() && a_node.path.compare(a_dataPath, Qt::CaseInsensitive) == 0;
    });
    if (found == _nodes.end()) {
        return {};
    }
    return createIndex(found->row, kNameColumn, static_cast<quintptr>(std::distance(_nodes.begin(), found)));
}

QModelIndex MeshTreeModel::index(const int a_row, const int a_column, const QModelIndex& a_parent) const {
    const auto& children = NodeOf(a_parent).children;
    if (a_row < 0 || std::cmp_greater_equal(a_row, children.size()) || a_column < 0 || a_column >= kColumns) {
        return {};
    }
    return createIndex(a_row, a_column, static_cast<quintptr>(children[a_row]));
}

QModelIndex MeshTreeModel::parent(const QModelIndex& a_index) const {
    const auto parent = a_index.isValid() ? NodeOf(a_index).parent : kRoot;
    if (parent <= kRoot) {
        return {};
    }
    return createIndex(_nodes[parent].row, kNameColumn, static_cast<quintptr>(parent));
}

int MeshTreeModel::rowCount(const QModelIndex& a_parent) const {
    return a_parent.column() > kNameColumn ? 0 : static_cast<int>(NodeOf(a_parent).children.size());
}

int MeshTreeModel::columnCount(const QModelIndex& a_parent) const {
    Q_UNUSED(a_parent);
    return kColumns;
}

QVariant MeshTreeModel::data(const QModelIndex& a_index, const int a_role) const {
    if (!a_index.isValid()) {
        return {};
    }

    const auto& node = NodeOf(a_index);
    const auto isMesh = !node.path.isEmpty();
    if (a_role == kFilterPathRole) {
        return node.path;
    }

    if (a_index.column() == kNameColumn) {
        switch (a_role) {
            case Qt::DisplayRole:    return node.name;
            case Qt::ToolTipRole:    return isMesh ? QDir::toNativeSeparators(node.path) : QVariant();
            case Qt::DecorationRole: return isMesh ? _meshIcon : _folderIcon;
            default:                 return {};
        }
    }

    if (a_role != Qt::DisplayRole || !isMesh || !_winnerLabel) {
        return {};
    }
    const auto nodeId = static_cast<int>(a_index.internalId());
    auto label = _winnerLabels.constFind(nodeId);
    if (label == _winnerLabels.cend()) {
        label = _winnerLabels.insert(nodeId, _winnerLabel(node.path));
    }
    return *label;
}

QVariant MeshTreeModel::headerData(const int a_section, const Qt::Orientation a_orientation, const int a_role) const {
    if (a_orientation != Qt::Horizontal || a_role != Qt::DisplayRole) {
        return {};
    }
    return a_section == kNameColumn ? tr("Name") : tr("Mod");
}
