#include <gtest/gtest.h>

#include <string>
#include <thread>
#include <vector>

#include "libca/fs/file_util.hpp"
#include "libca/json/kv_store.hpp"

using namespace ca;
using namespace ca::json;

namespace {

// 临时目录 RAII 守卫：每个用例独立目录，析构递归清理。
class TempDirGuard
{
public:
    TempDirGuard()
    {
        auto result = ca::fs::FileUtil::create_temp_directory("libca_json_kv_test_");
        if (result.is_ok()) {
            path_ = std::move(result.unwrap());
        }
    }

    ~TempDirGuard()
    {
        if (!path_.empty()) {
            ca::fs::FileUtil::remove_all(path_);
        }
    }

    std::string file(const std::string& name) const
    {
        return ca::fs::PathUtil::join(path_, name);
    }

private:
    std::string path_;
};

}  // namespace

// ============================================================================
// 基本读写与 round-trip
// ============================================================================

TEST(KvStoreTest, SetGetAllTypes)
{
    TempDirGuard tmp;
    KvStore store(tmp.file("store.json"));

    store.set_string("name", "张三");
    store.set_int("age", 30);
    store.set_bool("active", true);
    store.set_double("score", 97.5);

    EXPECT_EQ(store.get_string("name", ""), "张三");
    EXPECT_EQ(store.get_int("age", 0), 30);
    EXPECT_TRUE(store.get_bool("active", false));
    EXPECT_DOUBLE_EQ(store.get_double("score", 0.0), 97.5);

    EXPECT_TRUE(store.contains("name"));
    EXPECT_FALSE(store.contains("missing"));
    EXPECT_EQ(store.len(), 4u);
}

TEST(KvStoreTest, SaveAndReloadRoundtrip)
{
    TempDirGuard tmp;
    auto path = tmp.file("roundtrip.json");

    KvStore store(path);
    store.set_string("名字", "值\"含\"转义");
    store.set_int("n", -42);
    store.set_bool("b", true);
    store.set_double("pi", 3.141592653589793);
    ASSERT_TRUE(store.save().is_ok());

    // 文件确实落盘且内容含写入的键
    auto text = ca::fs::FileUtil::read_all_text(path);
    ASSERT_TRUE(text.is_ok());
    EXPECT_NE(text.unwrap().find("\"名字\""), std::string::npos);

    auto loaded = KvStore::load(path);
    ASSERT_TRUE(loaded.is_ok());
    auto store2 = std::move(loaded).unwrap();
    EXPECT_EQ(store2.get_string("名字", ""), "值\"含\"转义");
    EXPECT_EQ(store2.get_int("n", 0), -42);
    EXPECT_TRUE(store2.get_bool("b", false));
    EXPECT_DOUBLE_EQ(store2.get_double("pi", 0.0), 3.141592653589793);
    EXPECT_EQ(store2.len(), 4u);
    EXPECT_EQ(store2.path(), path);
}

TEST(KvStoreTest, SetOverwritesValueAndType)
{
    TempDirGuard tmp;
    KvStore store(tmp.file("overwrite.json"));

    store.set_int("k", 1);
    EXPECT_EQ(store.get_int("k", 0), 1);
    // 覆盖为字符串：类型一并替换，get_int 不再命中
    store.set_string("k", "one");
    EXPECT_EQ(store.get_string("k", ""), "one");
    EXPECT_EQ(store.get_int("k", -1), -1);
    // 再覆盖回整数
    store.set_int("k", 2);
    EXPECT_EQ(store.get_int("k", 0), 2);
    EXPECT_EQ(store.len(), 1u);
}

// ============================================================================
// 默认值语义
// ============================================================================

TEST(KvStoreTest, DefaultsOnMissingKey)
{
    TempDirGuard tmp;
    KvStore store(tmp.file("empty.json"));

    EXPECT_EQ(store.get_string("missing", "fallback"), "fallback");
    EXPECT_EQ(store.get_int("missing", 7), 7);
    EXPECT_FALSE(store.get_bool("missing", false));
    EXPECT_DOUBLE_EQ(store.get_double("missing", 0.25), 0.25);

    // Option 版本：缺失返回 None
    EXPECT_TRUE(store.get_string("missing").is_none());
    EXPECT_TRUE(store.get_int("missing").is_none());
    EXPECT_TRUE(store.get_bool("missing").is_none());
    EXPECT_TRUE(store.get_double("missing").is_none());

    EXPECT_EQ(store.len(), 0u);
}

TEST(KvStoreTest, TypeMismatchFallsBackToDefault)
{
    TempDirGuard tmp;
    KvStore store(tmp.file("typed.json"));
    store.set_string("s", "text");
    store.set_int("i", 5);
    store.set_bool("b", true);
    store.set_double("d", 1.5);

    // 严格类型：类型不符视同缺失
    EXPECT_EQ(store.get_int("s", -1), -1);
    EXPECT_EQ(store.get_bool("s", true), true);
    EXPECT_EQ(store.get_string("i", "no"), "no");
    EXPECT_EQ(store.get_double("s", -1.0), -1.0);

    // get_double 接受 Int（数值提升）
    EXPECT_DOUBLE_EQ(store.get_double("i", 0.0), 5.0);
    // get_int 不接受 Float（避免静默截断）
    EXPECT_EQ(store.get_int("d", -1), -1);
}

// ============================================================================
// load：缺文件 / 坏 JSON / 非 object 根 / 不支持的值类型
// ============================================================================

TEST(KvStoreTest, LoadMissingFileReturnsEmptyStore)
{
    TempDirGuard tmp;
    auto path = tmp.file("not_exist.json");

    auto result = KvStore::load(path);
    ASSERT_TRUE(result.is_ok());
    auto store = std::move(result).unwrap();
    EXPECT_EQ(store.len(), 0u);
    EXPECT_EQ(store.path(), path);

    // 缺文件得到的空存储可以保存并重新加载
    store.set_int("first", 1);
    ASSERT_TRUE(store.save().is_ok());
    auto reload = KvStore::load(path);
    ASSERT_TRUE(reload.is_ok());
    EXPECT_EQ(std::move(reload).unwrap().get_int("first", 0), 1);
}

TEST(KvStoreTest, LoadInvalidJsonFails)
{
    TempDirGuard tmp;
    auto path = tmp.file("broken.json");
    ASSERT_TRUE(ca::fs::FileUtil::write_text(path, "{ not valid json !!").is_ok());

    auto result = KvStore::load(path);
    ASSERT_TRUE(result.is_err());
    // 错误模型跟随 json 模块：ParseError 带人读消息（解析错误另有位置）
    const auto err = std::move(result).unwrap_err();
    EXPECT_FALSE(err.message.is_empty());
}

TEST(KvStoreTest, LoadNonObjectRootFails)
{
    TempDirGuard tmp;
    auto path = tmp.file("array_root.json");
    ASSERT_TRUE(ca::fs::FileUtil::write_text(path, "[1, 2, 3]").is_ok());

    auto result = KvStore::load(path);
    ASSERT_TRUE(result.is_err());
    EXPECT_FALSE(std::move(result).unwrap_err().message.is_empty());
}

TEST(KvStoreTest, LoadUnsupportedValueTypeFails)
{
    TempDirGuard tmp;
    auto path = tmp.file("nested.json");
    ASSERT_TRUE(ca::fs::FileUtil::write_text(path, R"({"ok": 1, "nested": {"a": 2}})").is_ok());

    auto result = KvStore::load(path);
    ASSERT_TRUE(result.is_err());
    EXPECT_NE(std::move(result).unwrap_err().message.to_std_string().find("nested"),
              std::string::npos);
}

TEST(KvStoreTest, LoadStripsUtf8Bom)
{
    TempDirGuard tmp;
    auto path = tmp.file("bom.json");
    ASSERT_TRUE(ca::fs::FileUtil::write_text(path, "\xEF\xBB\xBF{\"k\": \"v\"}").is_ok());

    auto result = KvStore::load(path);
    ASSERT_TRUE(result.is_ok()) << "带 BOM 的存储文件应能加载";
    EXPECT_EQ(std::move(result).unwrap().get_string("k", ""), "v");
}

// ============================================================================
// remove / save 后重读
// ============================================================================

TEST(KvStoreTest, RemovePersistsAfterSave)
{
    TempDirGuard tmp;
    auto path = tmp.file("remove.json");

    KvStore store(path);
    store.set_string("a", "1");
    store.set_string("b", "2");
    ASSERT_TRUE(store.remove("a"));
    EXPECT_FALSE(store.remove("a"));  // 再删返回 false
    EXPECT_EQ(store.len(), 1u);
    ASSERT_TRUE(store.save().is_ok());

    auto reload = KvStore::load(path);
    ASSERT_TRUE(reload.is_ok());
    auto store2 = std::move(reload).unwrap();
    EXPECT_FALSE(store2.contains("a"));
    EXPECT_EQ(store2.get_string("b", ""), "2");
}

TEST(KvStoreTest, DoubleRoundtripKeepsPrecision)
{
    TempDirGuard tmp;
    auto path = tmp.file("precision.json");

    KvStore store(path);
    store.set_double("v", 0.1);
    ASSERT_TRUE(store.save().is_ok());

    auto reload = KvStore::load(path);
    ASSERT_TRUE(reload.is_ok());
    auto store2 = std::move(reload).unwrap();
    EXPECT_DOUBLE_EQ(store2.get_double("v", 0.0), 0.1);

    // 二次保存重读仍无损（%.17g 序列化保证 round-trip）
    store2.set_double("v", store2.get_double("v", 0.0) * 3.0);
    ASSERT_TRUE(store2.save().is_ok());
    auto reload2 = KvStore::load(path);
    ASSERT_TRUE(reload2.is_ok());
    EXPECT_DOUBLE_EQ(std::move(reload2).unwrap().get_double("v", 0.0), 0.30000000000000004);
}

// ============================================================================
// 线程安全冒烟：并发 set/get/save 不崩溃、最终一致
// ============================================================================

TEST(KvStoreTest, ConcurrentAccessSmoke)
{
    TempDirGuard tmp;
    KvStore store(tmp.file("concurrent.json"));

    constexpr int kThreads = 4;
    constexpr int kIters = 200;

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&store, t, kIters] {
            for (int i = 0; i < kIters; ++i) {
                std::string key = "k" + std::to_string(t);
                store.set_int(key, i);
                store.get_int(key, -1);
                store.contains(key);
                if (i % 50 == 0) {
                    EXPECT_TRUE(store.save().is_ok());
                }
            }
        });
    }
    for (auto& th : threads) {
        th.join();
    }
    EXPECT_EQ(store.len(), static_cast<ca::usize>(kThreads));
}
