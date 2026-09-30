local port = path.join(os.scriptdir(), "port", "xmake.lua")

package("bodyslide-material-file")
set_homepage("https://github.com/ousnius/BodySlide-and-Outfit-Studio")
set_description("BodySlide's reader for Fallout 4 BGSM and BGEM material files")
set_license("GPL-3.0")
add_urls("https://github.com/ousnius/BodySlide-and-Outfit-Studio.git", { submodules = false })
add_versions("2026.09.29", "802a7a5f87f5e1dcb7b55a2b716b9a17c8d9390c")
-- The nifly package installs its headers under a nifly/ prefix.
add_patches(
    "2026.09.29",
    path.join(os.scriptdir(), "patches", "nifly-include.patch"),
    "f4d0d3426be51e60017ff3c3f7bb3bcfa97889405cc3582f4b94785aacc8ddee"
)
add_deps("nifly 2026.09.14")
on_install("windows|x64", function(package)
    os.cp(port, "xmake.lua")
    import("package.tools.xmake").install(package, { nifly_includedir = package:dep("nifly"):installdir("include") })
end)
