#include "Browser/Export.h"
#include "PreviewNif.h"

void PreviewNifShowBrowser(QWidget* a_parent) {
    if (const auto* const plugin = PreviewNif::Instance()) {
        plugin->ShowBrowser(a_parent);
    }
}
