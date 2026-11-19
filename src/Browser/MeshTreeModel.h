#pragma once

#include <QAbstractItemModel>
#include <QHash>
#include <QIcon>
#include <QStringList>

#include <cstddef>
#include <functional>
#include <vector>

// The meshes as a folder tree, with the mod or archive that supplies each one's winning copy.
class MeshTreeModel final : public QAbstractItemModel {
    Q_OBJECT

public:
    // Returns the display text for the winning copy of a data path.
    using WinnerLabel = std::function<QString(const QString& a_dataPath)>;

    // A mesh's data path, for filtering. Empty for folders.
    static constexpr int kFilterPathRole = Qt::UserRole;

    explicit MeshTreeModel(QObject* a_parent = nullptr);

    // Takes data paths with forward slashes that the model doesn't hold yet. The rows already there keep their
    // indexes, so a view keeps its expanded folders and its selection.
    void AddMeshes(const QStringList& a_paths);
    // Fills the Mod column, which is empty until this is set.
    void SetWinnerLabel(WinnerLabel a_winnerLabel);
    // Empty for a folder or an invalid index.
    [[nodiscard]] QString DataPath(const QModelIndex& a_index) const;
    [[nodiscard]] QModelIndex IndexOf(const QString& a_dataPath) const;
    [[nodiscard]] qsizetype MeshCount() const noexcept {
        return _meshCount;
    }

    [[nodiscard]] QModelIndex index(int a_row, int a_column, const QModelIndex& a_parent = {}) const override;
    [[nodiscard]] QModelIndex parent(const QModelIndex& a_index) const override;
    [[nodiscard]] int rowCount(const QModelIndex& a_parent = {}) const override;
    [[nodiscard]] int columnCount(const QModelIndex& a_parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& a_index, int a_role = Qt::DisplayRole) const override;
    [[nodiscard]] QVariant headerData(int a_section, Qt::Orientation a_orientation, int a_role) const override;

private:
    struct Node {
        QString name;
        // The data path of a mesh. Empty for a folder.
        QString path;
        int parent = -1;
        int row = 0;
        std::vector<int> children;
        // How many of the children, from the first, are in display order.
        std::ptrdiff_t sortedChildren = 0;
    };

    // Takes a folder's data path with a trailing slash, and creates the folders that are missing.
    [[nodiscard]] int FolderAt(const QString& a_folderPath, std::vector<int>& a_grown);
    int Append(Node a_node, std::vector<int>& a_grown);
    void SortChildren(std::vector<int>& a_folders);
    void MovePersistentIndexes();
    [[nodiscard]] const Node& NodeOf(const QModelIndex& a_index) const;

    // The first node is the root, which has no index of its own. Nodes are only ever appended, so a node's position
    // here identifies it for as long as the model lives.
    std::vector<Node> _nodes;
    // Lowercase folder path with a trailing slash to the folder's node.
    QHash<QString, int> _folders;
    // Fetched once. The style builds a standard icon from the shell on every request, which is slow for each row a
    // view paints.
    QIcon _folderIcon;
    QIcon _meshIcon;
    WinnerLabel _winnerLabel;
    mutable QHash<int, QString> _winnerLabels;
    qsizetype _meshCount = 0;
};
