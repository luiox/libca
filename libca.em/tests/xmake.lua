add_includedirs("$(projectdir)/src")

includes("em_base")
includes("em_component")
includes("em_crypto")
includes("em_dstream")
includes("em_format")
includes("em_log")
includes("em_motion")
includes("em_mpool")
includes("em_ota")
includes("em_platform")
includes("em_protocol")
includes("em_shell")
includes("em_util")

-- Optional: depends on external SDL packages.
if has_config("enable_em_eimui_test") then
    includes("em_eimui")
end
