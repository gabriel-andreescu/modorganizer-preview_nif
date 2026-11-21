#include "Data/WinnerPlan.h"

#include <QFileInfo>
#include <QObject>

namespace {
QString ArchiveBlocker(const QVector<DataFileSource>& a_copies, const qsizetype a_index) {
    // Archives follow the loose copies, latest loaded first, so an archive listed earlier loads after the chosen one.
    for (qsizetype i = 0; i < a_index; ++i) {
        const auto& copy = a_copies[i];
        if (copy.kind != DataFileKind::kArchive) {
            continue;
        }

        const auto archive = QFileInfo(copy.path).fileName();
        if (copy.plugin.isEmpty()) {
            return QObject::tr("%1 loads later and wins. The game's INI files load it.").arg(archive);
        }
        return QObject::tr("%1 loads later and wins. Load %2 earlier or disable it.").arg(archive, copy.plugin);
    }

    for (const auto& copy : a_copies) {
        if (copy.kind == DataFileKind::kLoose && copy.modName.isEmpty() && !copy.hidden) {
            return QObject::tr(
                "A loose copy in the game's Data folder wins over every archive. Only files in mods can be hidden."
            );
        }
    }
    return {};
}
} // namespace

WinnerPlan PlanWinner(const QVector<DataFileSource>& a_copies, const qsizetype a_index) {
    WinnerPlan plan;
    const auto& chosen = a_copies[a_index];
    if (chosen.hidden) {
        plan.toRestore.push_back(chosen);
    }
    if (chosen.kind == DataFileKind::kArchive) {
        plan.blocker = ArchiveBlocker(a_copies, a_index);
        if (!plan.blocker.isEmpty()) {
            return plan;
        }
    }

    // A loose copy in the game's Data folder ranks below every mod, so it never needs hiding for a mod's copy to win.
    for (qsizetype i = 0; i < a_copies.size(); ++i) {
        const auto& copy = a_copies[i];
        if (i != a_index && copy.kind == DataFileKind::kLoose && !copy.modName.isEmpty() && !copy.hidden) {
            plan.toHide.push_back(copy);
        }
    }
    return plan;
}
