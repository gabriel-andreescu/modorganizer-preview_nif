#include "Preview/CopyActionState.h"
#include "Data/WinnerPlan.h"
#include "Preview/CopyText.h"

#include <QObject>

CopyActionState CopyActionStateFor(const QVector<DataFileSource>& a_copies, const qsizetype a_index) {
    if (a_index < 0) {
        return {};
    }
    if (CopyText::RankOf(a_copies, a_index) == CopyText::Rank::kWinner) {
        return {.hideOthersToolTip = QObject::tr("This copy is already the winner")};
    }

    const auto hidden = a_copies[a_index].hidden;
    const auto plan = PlanWinner(a_copies, a_index);
    if (!plan.blocker.isEmpty()) {
        return {.hideOthersToolTip = plan.blocker, .canUnhide = hidden};
    }
    if (plan.toHide.isEmpty()) {
        return {.hideOthersToolTip = QObject::tr("No other loose copy to hide"), .canUnhide = hidden};
    }
    return {
        .canHideOthers = true,
        .hideOthersToolTip = hidden ? QObject::tr(
                                          "Unhide this copy and hide the other loose copies, so it becomes the winner"
                                      )
                                    : QObject::tr("Hide the other loose copies, so this one becomes the winner"),
        .canUnhide = hidden,
    };
}
