set_xmakever("3.1.1")
set_project("preview_nif")
set_policy("package.requires_lock", true)

local version = "0.6.0"

add_repositories("mopk https://github.com/gabriel-andreescu/ModOrganizerPluginKit")
add_repositories("xmake-luals https://github.com/gabriel-andreescu/xmake-luals.git")
add_addons("mopk 0.2.2", "xmake-luals 0.1.1")
includes("@addon/mopk/project", "@addon/mopk/native", "@addon/xmake-luals/luals")
includes("packages/*/*/xmake.lua")

-- Dependencies

add_requires(
    "devbench-api 0.1.0",
    "mo2-libbsarch 0.1.2",
    "bodyslide-material-file 2026.09.29",
    "eigen 3.4.0",
    "gli 2026.04.05",
    "nifly 2026.09.14",
    { system = false }
)

-- Build targets

target("Plugin", function()
    set_default(false)
    set_basename("preview_nif")
    set_version(version)
    add_rules("@addon/mopk/plugin")
    add_frameworks("QtOpenGL", "QtOpenGLWidgets")
    add_files("$(projectdir)/src/**.cpp", "$(projectdir)/src/**.h|PCH.h")
    add_includedirs("$(projectdir)/src")
    set_pcxxheader("$(projectdir)/src/PCH.h")
    add_packages(
        "qt6base",
        "mo2-uibase",
        "devbench-api",
        "mo2-libbsarch",
        "bodyslide-material-file",
        "eigen",
        "gli",
        "nifly"
    )
    add_rules("@devbench-api/integration")
end)

-- MO2 allows one plugin type per DLL, so the Tools menu entry is a plugin of its own.
target("BrowserTool", function()
    set_default(false)
    set_basename("preview_nif_browser")
    set_version(version)
    add_rules("@addon/mopk/plugin")
    add_files("$(projectdir)/browser_tool/**.cpp", "$(projectdir)/browser_tool/**.h")
    add_packages("qt6base", "mo2-uibase")
end)

-- Packages
target("preview_nif", function()
    set_version(version)
    add_rules("@addon/mopk/package", {
        targets = { "Plugin", "BrowserTool" },
        nexus = {
            game = "skyrimspecialedition",
            mod_id = "7318624410125",
            files = {
                ["2.5.2"] = {
                    file_id = "3216322",
                    category = "main",
                    primary = true,
                    display_name = "NIF Preview for MO2 2.5.2",
                    description = "Requires Mod Organizer 2.5.2\n\nNOT COMPATIBLE WITH: MO 2.5.3",
                },
                ["2.5.3beta12"] = {
                    file_id = "7399058",
                    category = "main",
                    display_name = "NIF Preview for MO2 2.5.3beta12",
                    description = "Requires Mod Organizer 2.5.3beta12\n\nNOT COMPATIBLE WITH: MO 2.5.2",
                },
            },
        },
    })
    add_installfiles("data/(**)", { prefixdir = "data" })
end)
