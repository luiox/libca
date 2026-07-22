target("libca.em_driver")
    set_kind("static")
    set_group("em")
    add_deps("libca.em_base")
    -- 仓库内部聚合 target，仅用于构建和测试。
    -- 外部项目应使用 import("libca.em") 和 em.add_libs(...)。
    add_files(path.join(os.scriptdir(), "**.c"))
    -- icm20948 尚无 manifest，并依赖未入库的 BSP，不进入默认聚合构建。
    remove_files(path.join(os.scriptdir(), "icm20948/**.c"))

