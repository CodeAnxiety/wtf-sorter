add_requires("fmt", "dimcli")
add_rules("mode.debug", "mode.release")
set_languages("c++20")

target("wtf-sorter")
    set_kind("binary")
    add_files("src/*.cpp")
    add_headerfiles("src/*.h")
    add_packages("fmt", "dimcli")
