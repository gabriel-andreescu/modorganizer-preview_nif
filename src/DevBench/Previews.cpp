#include "DevBench/Previews.h"
#include "Preview/Widget.h"

#include <QApplication>
#include <QStringList>

#include <algorithm>
#include <iterator>
#include <stdexcept>

namespace DevBench {

std::vector<Previews::Entry> Previews::Live() {
    std::erase_if(_tracked, [](const auto& a_entry) { return a_entry.second.isNull(); });

    for (const auto* const window : QApplication::topLevelWidgets()) {
        for (auto* const widget : window->findChildren<NifPreviewWidget*>()) {
            Track(widget);
        }
    }

    std::vector<Entry> entries;
    entries.reserve(_tracked.size());
    for (const auto& [previewId, widget] : _tracked) {
        entries.push_back({.id = previewId, .widget = widget});
    }
    return entries;
}

Previews::Entry Previews::Find(const std::optional<int> a_id) {
    const auto entries = Live();
    if (a_id) {
        const auto entry = std::ranges::find(entries, *a_id, &Entry::id);
        if (entry == entries.end()) {
            throw std::runtime_error(QStringLiteral("No live preview has id %1").arg(*a_id).toStdString());
        }
        return *entry;
    }

    std::vector<Entry> visible;
    std::ranges::copy_if(entries, std::back_inserter(visible), [](const Entry& a_entry) {
        return a_entry.widget->isVisible();
    });
    if (visible.size() == 1) {
        return visible.front();
    }

    QStringList ids;
    for (const auto& entry : visible) {
        ids.append(QString::number(entry.id));
    }
    throw std::runtime_error(
        visible.empty()
            ? std::string("No preview is visible")
            : QStringLiteral("Several previews are visible, pass one id: %1").arg(ids.join(", ")).toStdString()
    );
}

int Previews::Track(NifPreviewWidget* a_widget) {
    const auto tracked = std::ranges::find_if(_tracked, [&](const auto& a_entry) {
        return a_entry.second == a_widget;
    });
    if (tracked != _tracked.end()) {
        return tracked->first;
    }

    const auto previewId = _nextId++;
    _tracked.emplace_back(previewId, a_widget);
    return previewId;
}

}
