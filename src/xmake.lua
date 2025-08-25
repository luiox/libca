-- 最基础的库
target("ca-base")
    set_kind("static")
    add_files("base/**.c")
    add_headerfiles("base/**.h")
    add_includedirs(".", { public = true })
-- 嵌入式驱动库
target("ca-em_driver")
    set_kind("static")
    add_files("em_driver/**.c")
    add_headerfiles("base/**.h")

    add_deps("ca-base")
-- 嵌入式工具库
target("ca-em_util")
    set_kind("static")
    add_files("em_util/**.c")
    add_headerfiles("base/**.h")

    add_deps("ca-base")
