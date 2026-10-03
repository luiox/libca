#include <libca/test/test.hpp>

#include <algorithm>
#include <cstdlib>
#include <mutex>

namespace ca::test {

namespace {

std::filesystem::path& top_project()
{
    static std::filesystem::path instance;
    return instance;
}

std::filesystem::path& current_project()
{
    static std::filesystem::path instance;
    return instance;
}

// name → 项目根映射。setup 阶段一次性写入，之后只读。
std::unordered_map<std::string, std::filesystem::path>& name_to_path()
{
    static std::unordered_map<std::string, std::filesystem::path> instance;
    return instance;
}

// 扫描只允许执行一次：映射重建会破坏并发只读契约。
std::once_flag& scan_once()
{
    static std::once_flag flag;
    return flag;
}

#ifdef _WIN32
/// Windows：构造扩展长度（\\?\）路径供枚举使用——以宽字符链路根治共享临时根
/// 出现 ≥260 字符（UTF-16 计）长路径目录时的启动期枚举失败（#1126 同族缺陷：
/// 传统 MAX_PATH 上限 259，窄路径语义下底层 FindFirstFileW 直接失败）。
/// 非绝对路径或已带前缀时原样返回。
std::filesystem::path to_extended_length(const std::filesystem::path& path)
{
    std::wstring wide = path.wstring();
    if (wide.rfind(L"\\\\?\\", 0) == 0) return path;
    if (!path.is_absolute()) return path;
    std::replace(wide.begin(), wide.end(), L'/', L'\\');
    return std::filesystem::path(L"\\\\?\\" + wide);
}

/// 剥离 \\?\ 前缀还原常规路径（UNC 形式还原 \\server\\... 起始）。映射表对外
/// 存储的路径形态保持与旧实现一致，既有用例零变化。
std::filesystem::path from_extended_length(const std::filesystem::path& path)
{
    std::wstring wide = path.wstring();
    if (wide.rfind(L"\\\\?\\UNC\\", 0) == 0) {
        return std::filesystem::path(L"\\\\" + wide.substr(8));
    }
    if (wide.rfind(L"\\\\?\\", 0) == 0) return std::filesystem::path(wide.substr(4));
    return path;
}
#endif

/// 递归扫描 root 下全部 .project_root_file（内容首行 = 项目名），
/// 建立 name → 所在目录 映射；黑名单目录不深入。
///
/// Windows 上以扩展长度（\\?\）宽字符链路枚举（首选根治案）：长路径目录可
/// 正常遍历。其余枚举错误不再经 range-for 的抛异常 operator++ 变成启动期
/// 未捕获异常（进程 0xC0000409 硬崩），而是明确报错拒启（兜底案，与根治
/// 并存），错误模型与 setup() 的 std::runtime_error 约定一致。
void scan_marker_files(const std::filesystem::path& root)
{
    name_to_path().clear();
    std::error_code ec;
#ifdef _WIN32
    const std::filesystem::path scan_root = to_extended_length(root);
#else
    const std::filesystem::path scan_root = root;
#endif
    std::filesystem::recursive_directory_iterator iter(
        scan_root, std::filesystem::directory_options::skip_permission_denied, ec);
    if (ec) return;

    const std::filesystem::recursive_directory_iterator end{};
    while (iter != end) {
        const std::filesystem::directory_entry entry = *iter;
#ifdef _WIN32
        const std::filesystem::path entry_path = from_extended_length(entry.path());
#else
        const std::filesystem::path entry_path = entry.path();
#endif

        std::error_code entry_ec;
        const bool is_dir = entry.is_directory(entry_ec);
        if (entry_ec) {
            throw std::runtime_error("[libca.test] failed to stat directory entry: " +
                                     entry_path.string() + " (" + entry_ec.message() + ")");
        }

        if (is_dir) {
            auto name = entry_path.filename().string();
            if ((!name.empty() && name[0] == '.') || name.compare(0, 5, "build") == 0 ||
                name == "node_modules") {
                iter.disable_recursion_pending();
            }
        } else if (entry_path.filename() == ".project_root_file") {
            // 打开文件用枚举原生路径（长路径下经扩展前缀仍可读）。
            std::ifstream f(entry.path());
            std::string   content;
            std::getline(f, content);
            if (!content.empty()) {
                name_to_path()[content] = entry_path.parent_path();
            }
        }

        ec.clear();
        iter.increment(ec);
        if (ec) {
            throw std::runtime_error("[libca.test] failed to enumerate directory: " +
                                     entry_path.string() + " (" + ec.message() + ")");
        }
    }
}

std::vector<uint8_t> read_binary_file(const std::filesystem::path& path, const char* label)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error(std::string("[libca.test] cannot open ") + label + ": " +
                                 path.string());
    }
    in.seekg(0, std::ios::end);
    const auto tellg_val = in.tellg();
    if (tellg_val < 0) {
        throw std::runtime_error(std::string("[libca.test] failed to determine size of ") +
                                 label + ": " + path.string());
    }
    const auto           size = static_cast<size_t>(tellg_val);
    std::vector<uint8_t> data(size);
    in.seekg(0, std::ios::beg);
    in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
    return data;
}

/// 输出根：<top>/test，环境变量 LIBCA_TEST_OUT_ROOT 可覆盖。
std::filesystem::path out_root()
{
    if (const char* override_root = std::getenv("LIBCA_TEST_OUT_ROOT")) {
        if (override_root[0] != '\0') return std::filesystem::path(override_root);
    }
    return top_project() / "test";
}

std::filesystem::path ensure_dir(const std::filesystem::path& dir)
{
    std::error_code ec;
    if (!std::filesystem::exists(dir)) {
        std::filesystem::create_directories(dir, ec);
        if (ec) {
            throw std::runtime_error("[libca.test] failed to create directory: " + dir.string() +
                                     " (" + ec.message() + ")");
        }
    }
    return dir;
}

}   // anonymous namespace

void setup(const std::string& project)
{
    // 扫描仅首次执行（call_once 保证线程安全）；名字解析每次执行——
    // 未知名始终抛错，也允许启动阶段显式切换当前项目。
    std::call_once(scan_once(), [] {
        top_project() = std::filesystem::current_path();
        scan_marker_files(top_project());
    });

    const auto it = name_to_path().find(project);
    if (it == name_to_path().end()) {
        throw std::runtime_error("[libca.test] unknown project: " + project +
                                 " (no .project_root_file found with this name)");
    }
    current_project() = it->second;
}

std::filesystem::path top_project_path()
{
    return top_project();
}

std::filesystem::path current_project_path()
{
    return current_project();
}

std::filesystem::path project_path(const std::string& project)
{
    const auto it = name_to_path().find(project);
    if (it != name_to_path().end()) return it->second;
    // 回落：未命中按顶层同名目录猜（保持与既有布局的兼容性）。
    return top_project() / project;
}

bool has_project(const std::string& project)
{
    return name_to_path().find(project) != name_to_path().end();
}

std::filesystem::path resource_path(const std::string& rel)
{
    return current_project() / "test_resource" / rel;
}

bool has_resource(const std::string& rel)
{
    return std::filesystem::exists(resource_path(rel));
}

std::vector<uint8_t> resource(const std::string& rel)
{
    return read_binary_file(resource_path(rel), "resource");
}

std::filesystem::path project_resource_path(const std::string& project, const std::string& rel)
{
    return project_path(project) / "test_resource" / rel;
}

bool has_project_resource(const std::string& project, const std::string& rel)
{
    return std::filesystem::exists(project_resource_path(project, rel));
}

std::vector<uint8_t> project_resource(const std::string& project, const std::string& rel)
{
    return read_binary_file(project_resource_path(project, rel), "project resource");
}

std::filesystem::path out_path(const std::string& filename)
{
    return ensure_dir(out_root()) / filename;
}

std::filesystem::path demo_out_path(const std::string& filename)
{
    return ensure_dir(out_root() / "demo") / filename;
}

std::filesystem::path temp_out_path(const std::string& filename)
{
    return ensure_dir(out_root() / "tmp") / filename;
}

}   // namespace ca::test
