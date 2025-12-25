-- C的测试库，因为主要是用C测试，所以就归类到嵌入式库算了
target("ca-em_test")
    set_kind("static")
    add_files("**.c")
    add_deps("ca-em_base")
