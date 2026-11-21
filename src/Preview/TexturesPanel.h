#pragma once

#include "Render/TextureResults.h"
#include "Textures/Source.h"

#include <QWidget>

#include <optional>

class QComboBox;
class QLabel;
class QPushButton;
class TextureList;

// The pane's textures, the mod each one comes from, and the actions on the selected texture's shown copy.
class TexturesPanel final : public QWidget {
    Q_OBJECT

public:
    explicit TexturesPanel(QWidget* a_parent = nullptr);

    // The sources' first entry follows the game. A pick for a single texture shows the source as Custom.
    void SetSources(const TextureSourceSet& a_sources, int a_current, bool a_hasPicks);
    void SetTextures(const QVector<TextureResult>& a_results);
    // The other pane's textures in Split Preview, or none.
    void SetComparison(std::optional<QVector<TextureResult>> a_other);

signals:
    void SourceSelected(int a_index);
    void PreviewCopy(const QString& a_key, const QString& a_path);
    void HideOthers(const TextureResult& a_result, qsizetype a_copy);
    void Unhide(const TextureResult& a_result, qsizetype a_copy);

private:
    void Rebuild();
    void UpdateActions();
    void ShowContextMenu(const QPoint& a_position);
    void HideOthersOfCurrent();
    void UnhideCurrent();
    void OpenCurrentInExplorer() const;
    void CopyCurrentPath() const;
    [[nodiscard]] bool Differs(const TextureResult& a_result) const;

    QComboBox* _sourceCombo = nullptr;
    QLabel* _differLabel = nullptr;
    TextureList* _list = nullptr;
    QPushButton* _hideOthersButton = nullptr;
    QPushButton* _unhideButton = nullptr;
    QPushButton* _openButton = nullptr;

    QVector<TextureResult> _results;
    TextureSourceProvider _source;
    bool _hasPicks = false;
    std::optional<QVector<TextureResult>> _other;
};
