add_requires("fmt", { configs = { header_only = true } })
add_requires("spdlog", { configs = { header_only = true, fmt_external = true } })
add_requires("gtest")

target("libca.log")
	set_kind("static")
	set_group("core")
	set_languages("cxx17")

	add_files("logger.cpp", "spdlog_backend.cpp")
	add_headerfiles("*.hpp")

	add_packages("fmt", "spdlog", { public = true })

target("test-libca.log")
	set_kind("binary")
	set_languages("cxx17")
	set_group("core/test")

	add_files("test-logger.cpp")
	add_deps("libca.log")
	add_tests("default")

	add_packages("gtest", "fmt", "spdlog")
