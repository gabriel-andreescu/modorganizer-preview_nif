#pragma once

class QWidget;

// Opens the NIF Browser. MO2 allows one plugin type per DLL, so the tool plugin that adds the Tools menu entry is a
// second DLL, and it calls this by name.
extern "C" __declspec(dllexport) void PreviewNifShowBrowser(QWidget* a_parent);
