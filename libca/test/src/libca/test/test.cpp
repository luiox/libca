#include <libca/test/test.hpp>

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

/// 递归扫描 root 下全部 .project_root_file（内容首行 = 项目名），
/// 建立 name → 所在目录 映射；黑名单目录不深入。
void scan_marker_files(const std::filesystem::path& root)
{
    name_to_path().clear();
    std::error_code ec;
    auto            iter = std::filesystem::recursive_directory_iterator(
        root, std::filesystem::directory_options::skip_permission_denied, ec);
    if (ec)
        return;

    for (auto& entry : iter) {
        if (entry.is_directory()) {
            auto name = entry.path().filename().string();
            if ((!name.empty() && name[0] == '.') || name.compare(0, 5, "build") == 0 ||
                name == "node_modules") {
                iter.disable_recursion_pending();
            }
            continue;
        }
        if (entry.path().filename() != ".project_root_file")
            continue;

        std::ifstream f(entry.path());
        std::string   content;
        std::getline(f, content);
        if (!content.empty()) {
            name_to_path()[content] = entry.path().parent_path();
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
        throw std::runtime_error(std::string("[libca.test] failed to determine size of ") + label +
                                 ": " + path.string());
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
        if (override_root[0] != '\0')
            return std::filesystem::path(override_root);
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
    if (it != name_to_path().end())
        return it->second;
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
