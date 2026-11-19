#pragma once

#include <QString>

#include <memory>

namespace nifly {
class NifFile;
}

// Logs a problem with a NIF's own data as a warning. A view is rebuilt whenever its textures change, and the NIF's
// problems stay the same, so only the first view built for a NIF logs them.
void ReportMeshProblem(const QString& a_problem);

// Covers the building of one view's shapes. Views are built on the GUI thread.
class MeshProblemScope final {
public:
    explicit MeshProblemScope(const std::shared_ptr<nifly::NifFile>& a_nifFile);
    ~MeshProblemScope();
    MeshProblemScope(const MeshProblemScope&) = delete;
    MeshProblemScope(MeshProblemScope&&) = delete;
    MeshProblemScope& operator=(const MeshProblemScope&) = delete;
    MeshProblemScope& operator=(MeshProblemScope&&) = delete;
};
