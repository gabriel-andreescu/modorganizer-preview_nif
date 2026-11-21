#pragma once

#include <QPointer>

#include <optional>
#include <utility>
#include <vector>

class NifPreviewWidget;

namespace DevBench {

// Assigns ids to live preview widgets, which MO2's preview dialog creates and destroys.
class Previews final {
public:
    struct Entry {
        int id = 0;
        NifPreviewWidget* widget = nullptr;
    };

    [[nodiscard]] std::vector<Entry> Live();
    [[nodiscard]] Entry Find(std::optional<int> a_id);
    int Track(NifPreviewWidget* a_widget);

private:
    std::vector<std::pair<int, QPointer<NifPreviewWidget>>> _tracked;
    int _nextId = 1;
};

}
