#include "Nif/FaceBonesModel.h"

bool IsFaceBonesModelPath(const QString& a_dataPath) {
    return a_dataPath.endsWith("_facebones.nif", Qt::CaseInsensitive);
}
