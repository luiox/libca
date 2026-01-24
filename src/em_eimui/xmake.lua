-- libsdl ttf
add_requires("libsdl2", "libsdl2_ttf")

target("test-em_eimui")
    set_kind("binary")
    add_files("*.c")
    add_deps("ca-em_base")
    add_packages("libsdl2", "libsdl2_ttf")
    
    if is_plat("windows") then
        add_ldflags("/SUBSYSTEM:CONSOLE")
    end
