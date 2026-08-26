#pragma once

// libca.test：基于标记文件的多项目测试布局与样本定位（header-only）。
//
// 解决的问题：多子项目仓库中，各子项目的测试需要按名字取自己的或别的
// 子项目的 test_resource 样本、把产物写进统一的 test/ 输出根，而不必
// 硬编码相对路径。
//
// 约定：
// - 每个可定位的子项目根放一个 `.project_root_file` 文件，内容首行为项目名；
//   setup() 自 CWD 递归扫描建立 name → 根路径 映射。
// - 扫描黑名单：'.' 前缀目录、"build" 前缀目录、"node_modules"。
// - 测试 target 须在 xmake 中 set_rundir("$(projectdir)")，使 CWD = 顶层仓根。
// - 资源解析：<当前项目根>/test_resource/<rel>；跨项目访问按名字取任意子项目。
// - 输出约定：<顶层仓根>/test/（自动建目录）；可用环境变量 LIBCA_TEST_OUT_ROOT
//   覆盖输出根（CI/沙箱场景）。
//
// 错误模型：setup/读取失败抛 std::runtime_error——测试基建在错误时快速失败，
// 不引入 Result 传播负担。
//
// 线程性：setup 仅应在 main 启动阶段调用一次；此后全部接口只读，可并发调用。

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ca::test {

/// 初始化：以 CWD 为顶层仓根，扫描全部 .project_root_file 并选中当前子项目。
///
/// 首次调用执行扫描（进程内一次，线程安全）；名字解析每次执行，未命中抛异常，
/// 因此可在启动阶段显式切换当前项目。
///
/// @param project 当前子项目的 .project_root_file 名字。
void setup(const std::string& project);

/// 顶层仓根（setup 时的 CWD）。
std::filesystem::path top_project_path();

/// 当前子项目根（.project_root_file 所在目录）。
std::filesystem::path current_project_path();

/// 按名字取任意子项目根；未在映射中命中时回落 <top>/<name>。
std::filesystem::path project_path(const std::string& project);

/// 该名字是否已由 .project_root_file 注册（区别于回落猜测）。
bool has_project(const std::string& project);

/// 当前项目资源：<current>/test_resource/<rel>。
std::filesystem::path resource_path(const std::string& rel);
bool                  has_resource(const std::string& rel);
std::vector<uint8_t>  resource(const std::string& rel);

/// 跨项目资源：<project_path(project)>/test_resource/<rel>。
std::filesystem::path project_resource_path(const std::string& project, const std::string& rel);
bool                  has_project_resource(const std::string& project, const std::string& rel);
std::vector<uint8_t>  project_resource(const std::string& project, const std::string& rel);

/// 输出路径约定（目录不存在则自动创建）：
/// out_path → <out_root>/，demo_out_path → <out_root>/demo/，temp_out_path → <out_root>/tmp/
std::filesystem::path out_path(const std::string& filename);
std::filesystem::path demo_out_path(const std::string& filename);
std::filesystem::path temp_out_path(const std::string& filename);

}   // namespace ca::test
