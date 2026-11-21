#include "Preview/CopyText.h"

#include <QFileInfo>
#include <QObject>

namespace {
QString Name(const DataFileSource& a_copy) {
    if (a_copy.kind == DataFileKind::kLoose) {
        return CopyText::Owner(a_copy);
    }

    const auto archive = QFileInfo(a_copy.path).fileName();
    return a_copy.modName.isEmpty() ? archive : QObject::tr("%1 - %2").arg(CopyText::Owner(a_copy), archive);
}
} // namespace

namespace CopyText {

QString Owner(const DataFileSource& a_copy) {
    if (a_copy.modName.isEmpty()) {
        return QObject::tr("Game Data");
    }
    if (a_copy.modName.compare(QStringLiteral("overwrite"), Qt::CaseInsensitive) == 0) {
        return QObject::tr("Overwrite");
    }
    return a_copy.modName;
}

Rank RankOf(const QVector<DataFileSource>& a_copies, const qsizetype a_index) {
    if (a_copies[a_index].hidden) {
        return Rank::kHidden;
    }
    return a_index == 0 ? Rank::kWinner : Rank::kOverwritten;
}

QString Label(const QVector<DataFileSource>& a_copies, const qsizetype a_index) {
    auto name = Name(a_copies[a_index]);
    switch (RankOf(a_copies, a_index)) {
        case Rank::kWinner:      return name;
        case Rank::kOverwritten: return QObject::tr("%1 (overwritten)").arg(name);
        case Rank::kHidden:      return QObject::tr("%1 (hidden)").arg(name);
    }
    return name;
}

}
