#pragma once

#include "libca/ini/ini_reader.hpp"

#include <utility>

#include <gtest/gtest.h>

namespace ca::ini::test {

inline ca::str::Utf8StringRef ref(const char* text) {
    return ca::str::Utf8StringRef::from_cstr(text);
}

inline IniDocument read_ok(const char* text, const IniReaderOptions& options = {}) {
    auto result = IniReader::read(ref(text), options);
    if (result.is_err()) {
        ADD_FAILURE() << std::move(result).unwrap_err().message.to_std_string();
        return IniDocument();
    }
    return std::move(result).unwrap();
}

inline ca::str::Utf8StringRef get_ok(const IniDocument& document, const char* section, const char* key) {
    auto result = document.get(ref(section), ref(key));
    if (result.is_err()) {
        ADD_FAILURE() << std::move(result).unwrap_err().to_std_string();
        return {};
    }
    return std::move(result).unwrap();
}

}  // namespace ca::ini::test
