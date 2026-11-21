#pragma once

#include <QString>

#include <functional>

class QObject;

namespace MOBase {
class IOrganizer;
}

// Tells MO2 a mod's files changed and calls back once its refresh ends, while the context object still exists. MO2
// refreshes asynchronously, so its file lists are stale until then.
void RefreshModData(
    MOBase::IOrganizer& a_organizer,
    const QString& a_modName,
    const QObject* a_context,
    std::function<void()> a_done
);
