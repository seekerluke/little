target("little")
    set_kind("binary")
    add_files("*.c")
    add_files("src/*.c")
    add_includedirs("src")
    set_languages("c99")
    set_warnings("allextra", "pedantic", "error")
    if is_mode("debug") then
        set_symbols("debug")
    elseif is_mode("release") then
        set_strip("all")
    end
