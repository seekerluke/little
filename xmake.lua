target("little")
    set_kind("binary")
    add_files("*.c")
    add_files("src/*.c")
    add_includedirs("src")
    set_languages("c99")
    set_warnings("allextra", "pedantic", "error")
    set_rundir("$(projectdir)")
    if is_mode("debug") then
        set_symbols("debug")
    elseif is_mode("release") then
        set_strip("all")
    end

-- note that "xmake test" is unused
target("tests")
    set_kind("binary")
    add_files("tests/tests.cpp")
    add_files("src/*.c")
    add_includedirs("src")
    set_languages("c++17")
    set_warnings("allextra", "pedantic", "error")
    set_strip("all")
    set_rundir("$(projectdir)")
    if is_mode("debug") then
        set_symbols("debug")
    elseif is_mode("release") then
        set_strip("all")
    end
