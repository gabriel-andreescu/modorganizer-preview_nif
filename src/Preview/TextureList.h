#pragma once

#include "Render/TextureResults.h"

#include <QTreeWidget>

// One row per texture: its data path, the slots that use it, and the copy the preview shows. A texture with several
// copies offers them in a dropdown, highest priority first.
class TextureList final : public QTreeWidget {
    Q_OBJECT

public:
    struct Row {
        TextureResult result;
        // In Split Preview, the other pane shows another copy of this texture.
        bool differs = false;
    };

    explicit TextureList(QWidget* a_parent = nullptr);

    // The source name is the texture source's, for textures it has no copy of.
    void SetRows(const QVector<Row>& a_rows, const QString& a_sourceName);
    // The selected texture, or null.
    [[nodiscard]] const TextureResult* Current() const;
    [[nodiscard]] QSize sizeHint() const override;

signals:
    void CopyChosen(const QString& a_key, const QString& a_path);
    void CurrentTextureChanged();

private:
    void AddRow(int a_index, const QString& a_sourceName);
    void AddCopyCombo(QTreeWidgetItem* a_item, const TextureResult& a_result, const QString& a_sourceName);

    QVector<Row> _rows;
    int _rowHeight = 0;
};
