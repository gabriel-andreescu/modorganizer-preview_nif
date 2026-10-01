add_rules("mode.debug", "mode.release")
set_languages("c++20")

option("nifly_includedir", { showmenu = true, description = "The nifly package's include directory" })

target("bodyslide-material-file", function()
    set_kind("static")
    add_files("src/files/MaterialFile.cpp", "src/utils/StringStuff.cpp")
    add_headerfiles("src/files/MaterialFile.h", { prefixdir = "bodyslide" })
    add_includedirs("$(nifly_includedir)")
end)
