add_requires("fmt", { configs = { header_only = true } })
add_requires("spdlog", { configs = { header_only = true, fmt_external = true } })

target("libca.log")
	set_kind("static")
	set_languages("cxx17")

	add_files("logger.cpp", "spdlog_backend.cpp")
	add_headerfiles("*.hpp")

	add_packages("fmt", "spdlog", { public = true })
