#include "Data/ModDataRefresh.h"

#include <QObject>
#include <QPointer>

#include <uibase/imodlist.h>
#include <uibase/imoinfo.h>
#include <utility>

void RefreshModData(
    MOBase::IOrganizer& a_organizer,
    const QString& a_modName,
    const QObject* a_context,
    std::function<void()> a_done
) {
    a_organizer.onNextRefresh(
        [context = QPointer<const QObject>(a_context), done = std::move(a_done)] {
            if (context) {
                done();
            }
        },
        false
    );

    // MO2 refreshes every mod whichever one is passed, so a name without a mod entry, like overwrite, passes none.
    const auto* const modList = a_organizer.modList();
    a_organizer.modDataChanged(modList ? modList->getMod(a_modName) : nullptr);
}
