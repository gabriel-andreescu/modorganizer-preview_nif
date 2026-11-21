#include "Render/MeshProblems.h"

#include <nifly/NifFile.hpp>

#include <QtLogging>

#include <algorithm>
#include <vector>

namespace {
bool& Muted() {
    static bool muted = false;
    return muted;
}

// Records the NIF as logged and returns whether an earlier view already logged it.
bool AlreadyLogged(const std::shared_ptr<nifly::NifFile>& a_nifFile) {
    static std::vector<std::weak_ptr<nifly::NifFile>> logged;
    std::erase_if(logged, [](const auto& a_nif) { return a_nif.expired(); });
    if (std::ranges::any_of(logged, [&](const auto& a_nif) { return a_nif.lock() == a_nifFile; })) {
        return true;
    }
    logged.push_back(a_nifFile);
    return false;
}
} // namespace

void ReportMeshProblem(const QString& a_problem) {
    if (!Muted()) {
        qWarning("%s", qUtf8Printable(a_problem));
    }
}

MeshProblemScope::MeshProblemScope(const std::shared_ptr<nifly::NifFile>& a_nifFile) {
    Muted() = AlreadyLogged(a_nifFile);
}

MeshProblemScope::~MeshProblemScope() {
    Muted() = false;
}
