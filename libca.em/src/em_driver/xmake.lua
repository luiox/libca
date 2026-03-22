target("libca.em_driver")
    set_kind("static")
    add_deps("libca.em_base")
    -- Internal aggregate target for repository build/tests.
    -- External projects should prefer import("libca.em") + em.add_libs(...).
    add_files(path.join(os.scriptdir(), "**.c"))

