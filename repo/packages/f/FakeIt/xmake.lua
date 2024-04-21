package("FakeIt")

    set_kind("library", {headeronly = true})
    set_homepage("https://github.com/eranpeer/FakeIt")
    set_description("C++ mocking made easy. A simple yet very expressive, headers only library for c++ mocking.")

    set_urls("https://github.com/eranpeer/FakeIt.git")

    on_install(function (package)
        os.cp("FakeIt", package:installdir("include"))
    end)

    on_test(function (package)
        assert(package:check_cxxsnippets({test = [[
            FakeIt is a simple mocking framework for C++. It supports GCC, Clang and MS Visual C++.
        ]]}, {configs = {languages = "c++11"} } ))
    end)
package_end()