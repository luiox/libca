#include <libca/fs/stdio_file.hpp>
#include <libca/json/json.hpp>

int main()
{
    auto result = ca::json::JsonReader::read(
        ca::str::Utf8StringRef::from_cstr(R"({"installed":true})"));
    if (!result.is_ok())
        return 1;

    // fs::stdio_file 安装头自检：头可独立编译、符号可链接（open_stdio/Path），
    // 打开不存在文件须返回 FileNotFound（而非崩溃/成功）。
    const auto opened = ca::fs::open_stdio(
        ca::fs::Path::from_utf8_lossy("install_consumer_no_such_file"), "rb");
    return opened.is_err() && opened.unwrap_err() == ca::fs::FsError::FileNotFound ? 0 : 1;
}
