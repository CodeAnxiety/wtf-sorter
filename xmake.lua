add_requires("fmt >=8.1.1", "lyra >=1.6")
add_rules("mode.debug", "mode.release")
set_languages("c++20")

target("wtf-sorter")
    set_kind("binary")
    add_files("src/*.cpp")
    add_headerfiles("src/*.h")
    add_packages("fmt", "lyra")
