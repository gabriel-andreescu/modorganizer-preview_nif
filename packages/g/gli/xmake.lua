package("gli")
set_kind("library", { headeronly = true })
set_homepage("https://github.com/g-truc/gli")
set_description("OpenGL Image (GLI)")
set_license("MIT")
add_urls("https://github.com/g-truc/gli.git")
add_versions("2026.04.05", "3542f8830178061e0661f5df1e89d36ba7d7b0ab")
-- clang-tidy and clangd parse gli with Clang 22, which rejects its enum arithmetic and swizzle table.
add_patches(
    "2026.04.05",
    path.join(os.scriptdir(), "patches", "clang22-enum-bitmask.patch"),
    "884c4377dd7f174afb022436b5925e02cc601a3015f9331f91280a3d43201c3e"
)
add_patches(
    "2026.04.05",
    path.join(os.scriptdir(), "patches", "clang-tidy-texture-cache.patch"),
    "771ea03b60ea078c67dd9cb3ac358b40be61bc3a661e388c2da233703e6533b9"
)
add_patches(
    "2026.04.05",
    path.join(os.scriptdir(), "patches", "clang-tidy-swizzle-translate.patch"),
    "a12621a0b0eb20f226b99ffa6e2b8880e22bdf696ee6cfce46bb738a6a793f55"
)
add_deps("glm")
on_install(function(package)
    os.cp("gli", package:installdir("include"))
end)
