#pragma once

#include <QSortFilterProxyModel>

// Keeps the meshes whose data path contains the filter text, and the folders that lead to them.
class MeshFilterModel final : public QSortFilterProxyModel {
    Q_OBJECT

public:
    explicit MeshFilterModel(QObject* a_parent = nullptr);

    void SetFilterText(const QString& a_text);
    [[nodiscard]] bool HasFilter() const {
        return !_needle.isEmpty();
    }

protected:
    [[nodiscard]] bool filterAcceptsRow(int a_sourceRow, const QModelIndex& a_sourceParent) const override;

private:
    QString _needle;
};
