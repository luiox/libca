#include "libca/zip/detail/file_io.hpp"

#include <cstring>

#include "libca/str/os_string.hpp"

namespace ca::zip {

std::FILE* open_stdio_file(const std::string& path, const char* mode)
{
#if defined(_WIN32)
    // 宽化 mode：仅含 ASCII（"rb"/"wb" 等），逐字符拓宽即可。
    const usize      mode_len  = std::strlen(mode);
    std::wstring     wide_mode(mode_len, L'\0');
    for (usize i = 0; i < mode_len; ++i)
        wide_mode[i] = static_cast<wchar_t>(static_cast<unsigned char>(mode[i]));

    const str::OsString native = str::OsString::from_utf8_lossy(path);
    const auto          wide   = native.as_wide();
#if defined(_MSC_VER)
    std::FILE* fp = nullptr;
    if (::_wfopen_s(&fp, wide.data(), wide_mode.c_str()) != 0)
        return nullptr;
    return fp;
#else
    return ::_wfopen(wide.data(), wide_mode.c_str());
#endif
#else
    return std::fopen(path.c_str(), mode);
#endif
}

}   // namespace ca::zip
