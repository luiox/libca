-- 最基础的库
target("ca-base")
    set_kind("static")
    add_files("**.cpp")
    add_includedirs(".", { public = true })
