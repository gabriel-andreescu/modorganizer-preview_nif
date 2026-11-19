#include "Browser/MeshFilterModel.h"
#include "Browser/MeshTreeModel.h"

#include <QDir>

MeshFilterModel::MeshFilterModel(QObject* a_parent)
    : QSortFilterProxyModel(a_parent) {
    setRecursiveFilteringEnabled(true);
    // The Mod column's text arrives after the rows. Naming the role the filter reads keeps that change from running
    // the filter again on every folder above each row.
    setFilterRole(MeshTreeModel::kFilterPathRole);
}

void MeshFilterModel::SetFilterText(const QString& a_text) {
    const auto needle = QDir::fromNativeSeparators(a_text.trimmed()).toLower();
    if (needle == _needle) {
        return;
    }

    _needle = needle;
    invalidateFilter();
}

// A folder is rejected here and kept by recursive filtering when one of its meshes is accepted.
bool MeshFilterModel::filterAcceptsRow(const int a_sourceRow, const QModelIndex& a_sourceParent) const {
    if (_needle.isEmpty()) {
        return true;
    }

    const auto index = sourceModel()->index(a_sourceRow, 0, a_sourceParent);
    return index.data(MeshTreeModel::kFilterPathRole).toString().contains(_needle, Qt::CaseInsensitive);
}
